// dynamic_fanout_epoll.cpp
// Build: g++ -std=c++17 -O2 dynamic_fanout_epoll.cpp -lpthread

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <optional>

struct Event { uint64_t seq; int value; };

class FanoutRing {
public:
    FanoutRing(size_t capacity)
        : cap(capacity), buf(capacity), writeIndex(0) {}

    // Producer push: uses snapshot of read pointers to check overflow and publish
    bool push(const Event &e, const std::vector<std::atomic<size_t>*> &readPtrsSnapshot) {
        size_t w = writeIndex.load(std::memory_order_relaxed);
        size_t next = w + 1;

        // overflow check against snapshot of consumers
        for (auto rp : readPtrsSnapshot) {
            size_t rv = rp->load(std::memory_order_acquire);
            if (next - rv > cap) return false; // would overwrite unread data
        }

        buf[w % cap] = e;
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    // Consumer pop: consumerId is not needed if using pointer directly
    bool pop(std::atomic<size_t> &readIndex, Event &out) {
        size_t r = readIndex.load(std::memory_order_relaxed);
        size_t w = writeIndex.load(std::memory_order_acquire);
        if (r < w) {
            out = buf[r % cap];
            readIndex.store(r + 1, std::memory_order_release);
            return true;
        }
        return false;
    }

private:
    const size_t cap;
    std::vector<Event> buf;
    std::atomic<size_t> writeIndex;
};

// utility
int create_eventfd() {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd == -1) { perror("eventfd"); exit(1); }
    return efd;
}

// Consumer control structure
struct Consumer {
    int efd;
    std::atomic<size_t>* readPtr;
    std::thread thr;
    std::atomic<bool> running;
};

class FanoutManager {
public:
    FanoutManager(size_t capacity) : ring(capacity) {}

    // Add a consumer at runtime. Returns consumer index.
    size_t add_consumer() {
        auto *rp = new std::atomic<size_t>(0);
        int efd = create_eventfd();
        auto c = std::make_unique<Consumer>();
        c->efd = efd;
        c->readPtr = rp;
        c->running.store(true);

        // create consumer thread
        std::thread t(&FanoutManager::consumer_loop, this, rp, efd, &(c->running));

        std::lock_guard<std::mutex> lg(mu);
        c->thr = std::move(t);
        consumers.push_back(std::move(c));
        // return index
        return consumers.size() - 1;
    }

    // Remove consumer by index (waits for thread to exit)
    void remove_consumer(size_t idx) {
        std::unique_ptr<Consumer> local;
        {
            std::lock_guard<std::mutex> lg(mu);
            if (idx >= consumers.size()) return;
            // mark running false so consumer loop can exit
            consumers[idx]->running.store(false);
            // signal the consumer so it wakes up
            uint64_t inc = 1;
            write(consumers[idx]->efd, &inc, sizeof(inc));
            // move out the Consumer to join outside lock
            local = std::move(consumers[idx]);
            // remove from vector (swap-pop)
            if (idx + 1 != consumers.size()) consumers[idx] = std::move(consumers.back());
            consumers.pop_back();
        }
        // join and cleanup
        if (local->thr.joinable()) local->thr.join();
        close(local->efd);
        delete local->readPtr;
        // unique_ptr will auto-delete the consumer
    }

    // Producer thread: pushes events and signals all current consumers
    void producer_loop() {
        uint64_t seq = 1;
        while (seq <= 200000) {
            Event ev{seq, static_cast<int>(seq % 1000)};

            // take snapshot of read pointers and efds
            std::vector<std::atomic<size_t>*> readPtrs;
            std::vector<int> efds;
            {
                std::lock_guard<std::mutex> lg(mu);
                readPtrs.reserve(consumers.size());
                efds.reserve(consumers.size());
                for (auto &c : consumers) {
                    readPtrs.push_back(c->readPtr);
                    efds.push_back(c->efd);
                }
            }

            // push with snapshot
            while (!ring.push(ev, readPtrs)) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }

            // signal snapshot efds
            for (int efd : efds) {
                uint64_t inc = 1;
                ssize_t s = write(efd, &inc, sizeof(inc));
                (void)s;
            }

            seq++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // termination sentinel
        Event term{0,0};
        // snapshot again
        std::vector<std::atomic<size_t>*> readPtrs;
        std::vector<int> efds;
        {
            std::lock_guard<std::mutex> lg(mu);
            for (auto &c : consumers) { readPtrs.push_back(c->readPtr); efds.push_back(c->efd); }
        }
        while (!ring.push(term, readPtrs)) std::this_thread::sleep_for(std::chrono::microseconds(50));
        for (int efd : efds) { uint64_t inc = 1; write(efd, &inc, sizeof(inc)); }
    }

private:
    // consumer loop: each consumer reads its own readPtr and efd
    void consumer_loop(std::atomic<size_t>* readPtr, int efd, std::atomic<bool> *runningFlag) {
        int epfd = epoll_create1(0);
        if (epfd == -1) { perror("epoll_create1"); return; }
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = efd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, efd, &ev) == -1) { perror("epoll_ctl"); close(epfd); return; }

        bool running = true;
        while (running && runningFlag->load()) {
            struct epoll_event events[4];
            int n = epoll_wait(epfd, events, 4, -1);
            if (n == -1) {
                if (errno == EINTR) continue;
                perror("epoll_wait"); break;
            }
            for (int i = 0; i < n; ++i) {
                if (events[i].events & EPOLLIN) {
                    uint64_t counter;
                    ssize_t s = read(efd, &counter, sizeof(counter));
                    if (s == -1 && errno != EAGAIN) perror("read eventfd");
                    Event evnt;
                    while (ring.pop(*readPtr, evnt)) {
                        if (evnt.seq == 0) { running = false; break; }
                        std::cout << "Consumer(" << efd << ") seq=" << evnt.seq << " val=" << evnt.value << "\n";
                    }
                }
            }
        }
        close(epfd);
    }

    FanoutRing ring;
    std::mutex mu;
    std::vector<std::unique_ptr<Consumer>> consumers;
};

// Example main: add consumers dynamically
int main() {
    FanoutManager mgr(10240);

    // add two consumers initially
    mgr.add_consumer();
    mgr.add_consumer();

    // start producer in a thread
    std::thread p(&FanoutManager::producer_loop, &mgr);

    // after some time add another consumer dynamically
    std::this_thread::sleep_for(std::chrono::seconds(1));
    size_t newId = mgr.add_consumer();
    std::cout << "Added consumer id=" << newId << "\n";

    // let it run, then remove the last consumer
    std::this_thread::sleep_for(std::chrono::seconds(2));
    mgr.remove_consumer(newId);
    std::cout << "Removed consumer id=" << newId << "\n";

    p.join();

    // remove remaining consumers (cleanup)
    // remove in reverse order to avoid index shifting issues in this demo
    mgr.remove_consumer(0);
    mgr.remove_consumer(0);

    std::cout << "Done\n";
    return 0;
}
