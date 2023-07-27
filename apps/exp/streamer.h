#include "gst/gst.h"
class Streamer {
   public:
    Streamer(int argc, char *argv[]);
    ~Streamer();

    static void pad_added_handler(GstElement *src, GstPad *new_pad, Streamer *data);
    static void handle_message(Streamer *data, GstMessage *msg);

   private:
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

    gboolean playing = FALSE;
    gboolean seek_enabled = FALSE;
    gboolean seek_done = FALSE;
    gint64 duration = GST_CLOCK_TIME_NONE;
};

