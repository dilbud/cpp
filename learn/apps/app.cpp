extern "C" {
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
}

#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "ThreadPool.h"

int main() {
    ThreadPool pool;

    std::vector<std::future<size_t>> vec;

    size_t max = 500000;

    // Enqueue tasks for execution
    for (size_t i = 0; i < max; ++i) {
        auto func = [i] {
            // std::cout << "Task " << i << " is running on thread " << std::this_thread::get_id() << std::endl;
            // Simulate some work
            // std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return i;
        };
        vec.push_back(pool.enqueue(func));
    }

    for (size_t i = 0; i < max; i++) {
        std::printf("%zd\n", vec.at(i).get());
    }

    return 0;
}