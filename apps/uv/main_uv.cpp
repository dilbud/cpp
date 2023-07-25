#include <stdio.h>
#include <iostream>
#include <string>

#include <memory>
#include <vector>

#include "uv.h"
#include "Peer.h"

int main(int, char **) {
    std::string str("hello");
    for (auto &&i : str) {
        i = std::toupper(i);
    }
    std::printf("%s\n", str.c_str());

    uv_loop_t *loop = uv_default_loop();

    Peer p(loop);

    uv_run(loop, UV_RUN_DEFAULT);
    uv_loop_close(loop);
    return 0;
}
