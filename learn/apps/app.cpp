#include <event2/event.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <iostream>

void event_handler(evutil_socket_t fd, short events, void *arg) {
    uint64_t count;
    ssize_t read_size = read(fd, &count, sizeof(uint64_t));
    if (read_size == sizeof(uint64_t)) {
        printf("Eventfd triggered with count: %llu\n", count);
    } else {
        perror("read");
    }
}

int run01() {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd == -1) {
        perror("eventfd");
        return 1;
    }
    struct event_base *base = event_base_new();
    struct event *ev = event_new(base, efd, EV_READ | EV_PERSIST, event_handler, NULL);
    event_add(ev, NULL);
    for (;;) {
        uint64_t u = 1;

        ssize_t write_size = write(efd, &u, sizeof(uint64_t));
        if (write_size != sizeof(uint64_t)) {
            if (errno == EAGAIN) {
                printf("Eventfd is full on write attempt %d\n", 1);
            } else {
                perror("write");
                return 1;
            }
        } else {
            // printf("Successfully wrote to eventfd on write attempt %d\n", 1);
        }
        event_base_loop(base, EVLOOP_NONBLOCK);
    }
    event_base_dispatch(base);
    event_free(ev);
    event_base_free(base);
    close(efd);
    return 0;
}

////////////////////////////////////////////////////////////////////////

#define THREAD_COUNT 5
#define BUFFER_SIZE 128
using thread_data_t = struct {
    int id;
    int efd;
    struct event_base *base;
    struct event *ev;
    int *efd_array;
    char buffer[BUFFER_SIZE];
};
void event_handler02(evutil_socket_t fd, short events, void *arg) {
    uint64_t count;
    ssize_t read_size = read(fd, &count, sizeof(uint64_t));
    if (read_size == sizeof(uint64_t)) {
        thread_data_t *data = (thread_data_t *)arg;
        printf("Thread %d (Thread ID: %lu) received event with count: %llu and message: %s\n", data->id, pthread_self(),
               count, data->buffer);
    } else {
        perror("read");
    }
}
void *thread_function(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    data->ev = event_new(data->base, data->efd, EV_READ | EV_PERSIST, event_handler02, data);
    if (!data->ev) {
        fprintf(stderr, "Could not create event for thread %d\n", data->id);
        return NULL;
    }
    if (event_add(data->ev, NULL) == -1) {
        fprintf(stderr, "Could not add event for thread %d\n", data->id);
        return NULL;
    }
    // Broadcast to other threads
    for (int i = 0;; ++i) {
        snprintf(data->buffer, BUFFER_SIZE, "Message from thread %d", data->id);
        for (int j = 0; j < THREAD_COUNT; ++j) {
            if (data->id != j) {
                uint64_t u = 1;
                ssize_t write_size = write(data->efd_array[j], &u, sizeof(uint64_t));
                if (write_size != sizeof(uint64_t)) {
                    if (errno == EAGAIN) {
                        printf("Thread %d: Eventfd of thread %d is full, try again later\n", data->id, j);
                    } else {
                        perror("write");
                    }
                }
            }
        }
        // Process events
        event_base_loop(data->base, EVLOOP_NONBLOCK);
        sleep(1);
        // Optional delay for demonstration purposes
    }
    // Ensure all events are dispatched before exiting
    event_base_dispatch(data->base);
    event_free(data->ev);
    return NULL;
}
int run02() {
    thread_data_t thread_data[THREAD_COUNT];
    pthread_t threads[THREAD_COUNT];
    int efd_array[THREAD_COUNT];
    // Initialize eventfd and event base for each thread
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_data[i].id = i;
        thread_data[i].efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        efd_array[i] = thread_data[i].efd;
        if (thread_data[i].efd == -1) {
            perror("eventfd");
            exit(EXIT_FAILURE);
        }
        thread_data[i].base = event_base_new();
        if (!thread_data[i].base) {
            fprintf(stderr, "Could not initialize libevent base for thread %d\n", i);
            close(thread_data[i].efd);
            exit(EXIT_FAILURE);
        }
        thread_data[i].efd_array = efd_array;
        pthread_create(&threads[i], NULL, thread_function, &thread_data[i]);
    }
    // Wait for threads to complete
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(threads[i], NULL);
        event_base_free(thread_data[i].base);
        close(thread_data[i].efd);
    }
    return 0;
}
int main(int argc, char **argv) { return run02(); }