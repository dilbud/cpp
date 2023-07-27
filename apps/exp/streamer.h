#include "gst/gst.h"
class Streamer {
   public:
    Streamer(int argc, char *argv[]);
    ~Streamer();

   public:
    GstElement *pipeline;
    GstElement *source;
    GstElement *filter;
    GstElement *convert01;
    GstElement *convert02;
    GstElement *sink;

    GstElement *a_convert;
    GstElement *a_resample;
    GstElement *a_sink;

    GstBus *bus;
    GstMessage *msg;

    gboolean terminate = FALSE;
};

