#include "gst/gst.h"
class Streamer {
   public:
    Streamer(int argc, char *argv[]);
    ~Streamer();

    static void pad_added_handler(GstElement *src, GstPad *new_pad, Streamer *data);
    static void handle_message(Streamer *data, GstMessage *msg);

    static gboolean print_field(GQuark field, const GValue *value, gpointer pfx);
    static void print_caps(const GstCaps *caps, const gchar *pfx);
    static void print_pad_templates_information(GstElementFactory *factory);
    static void print_pad_capabilities(GstElement *element, gchar *pad_name);

   private:
    GstElement *pipeline;

    GstElementFactory *source_factory;
    GstElementFactory *sink_factory;

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

