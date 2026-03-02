#include <stdio.h>


struct AppContext {
    const char *src_filename = "/workspaces/cpp/video_data/sintel-short.mp4";
    const char *chunk_dir = "/workspaces/cpp/learn/apps/chunk/out";
};




int main(int argc, char **argv) {
    printf("Hello, FFmpeg!\n");

    AppContext appCtx;
}