#include <stdio.h>

#include <string>

#include "boost/thread/thread.hpp"
#include "libavcodec/avcodec.h"
#include "streamer.h"

void my_func() {
    std::string str("hello  thread");
    for (auto &&i : str) {
        i = std::toupper(i);
    }
    std::printf("%s\n", str.c_str());
}

int main(int argc, char ** argv) {
    std::string str("hello");
    for (auto &&i : str) {
        i = std::toupper(i);
    }
    std::printf("%s\n", str.c_str());
    boost::thread t(my_func);
    t.join();
    t.detach();
    // tutorial_main (argc, argv);
    Streamer stm(argc, argv);
    return 0;
}