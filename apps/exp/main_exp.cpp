#include <stdio.h>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>

// #include "boost/thread/thread.hpp"
// #include "libavcodec/avcodec.h"
#include "streamer.h"
// #include <boost/algorithm/string.hpp>

// extern "C" {
//     #include "cmdutils.h"
//     #include "ffmpeg.h"
// }

// void my_func() {
//     std::string str("hello  thread");
//     for (auto &&i : str) {
//         i = std::toupper(i);
//     }
//     std::printf("%s\n", str.c_str());
// }

// char *convert(const std::string &s)
// {
//     char *pc = new char[s.size() + 1];
//     strcpy(pc, s.c_str());
//     return pc;
// }

int main(int argc, char ** argv) {
    // char* pp = argv[0];
    // std::string str("hello");
    // for (auto &&i : str) {
    //     i = std::toupper(i);
    // }
    // std::printf("%s\n", str.c_str());
    // boost::thread t(my_func);
    // t.join();
    // t.detach();
    // tutorial_main (argc, argv);

    // std::string line = std::string(argv[0]) + " " + "-i /workspaces/cpp/video_data/sintel-short.mp4 -c:v libx264 -preset ultrafast -tune zerolatency -f mpegts udp://localhost:8500";
    // std::vector<std::string> strs;
    // boost::split(strs, line, boost::is_any_of(" "));

    // std::vector<char *> vc;
    // std::transform(strs.begin(), strs.end(), std::back_inserter(vc), convert);

    // char* char_arr[vc.size()];
    // for (size_t i = 0; i < vc.size(); i++) {
    //     char_arr[i] = vc.at(i);
    // }

    // int a = main_ex(strs.size(), char_arr);
    Streamer stm(argc, argv);
    return 0;
}