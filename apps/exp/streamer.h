#include "gst/gst.h"
class Streamer {
   public:
    Streamer(int argc, char *argv[]);
    ~Streamer();

   private:
    GstElement *pipeline;
    GstElement *source;
    GstElement *filter;
    GstElement *convert01;
    GstElement *convert02;
    GstElement *sink;
    GstBus *bus;
    GstMessage *msg;
};

