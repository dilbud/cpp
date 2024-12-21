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

int main(int argc, char **argv) {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd == -1) {
        perror("eventfd");
        return 1;
    }
    struct event_base *base = event_base_new();
    struct event *ev = event_new(base, efd, EV_READ | EV_PERSIST, event_handler, NULL);
    event_add(ev, NULL);
    for (; ; ) {
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
