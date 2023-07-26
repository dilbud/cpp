#include "streamer.h"

Streamer::Streamer(int argc, char *argv[]) {
    GstStateChangeReturn ret;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the elements */
    source = gst_element_factory_make("videotestsrc", "source01");
    convert01 = gst_element_factory_make("videoconvert", "convert01");
    filter = gst_element_factory_make("vertigotv", "filter01");
    convert02 = gst_element_factory_make("videoconvert", "convert02");
    sink = gst_element_factory_make("autovideosink", "sink01");

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("pipe");

    if (!pipeline || !source || !filter || !convert01 || !convert02 || !sink) {
        g_printerr("Not all elements could be created.\n");
        goto END;
    }

    /* Build the pipeline */
    gst_bin_add_many(
        GST_BIN(pipeline), 
        source, 
        convert01,
        filter, 
        convert02, 
        sink, 
        NULL);

    if (gst_element_link(source, convert01) != TRUE) {
        g_printerr("Elements 01 could not be linked.\n");
        gst_object_unref(pipeline);
        goto END;
    }
    if (gst_element_link(convert01, filter) != TRUE) {
        g_printerr("Elements 02 could not be linked.\n");
        gst_object_unref(pipeline);
        goto END;
    }
    if (gst_element_link(filter, convert02) != TRUE) {
        g_printerr("Elements 03 could not be linked.\n");
        gst_object_unref(pipeline);
        goto END;
    }
    if (gst_element_link(convert02, sink) != TRUE) {
        g_printerr("Elements 04 could not be linked.\n");
        gst_object_unref(pipeline);
        goto END;
    }

    /* Modify the source's properties */
    g_object_set(source, "pattern", 0, NULL);

    /* Start playing */
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        goto END;
    }

    /* Wait until error or EOS */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                     GstMessageType (GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    /* Parse message */
    if (msg != NULL) {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n",
                           GST_OBJECT_NAME(msg->src), err->message);
                g_printerr("Debugging information: %s\n",
                           debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                break;
            default:
                /* We should not reach here because we only asked for ERRORs and
                 * EOS */
                g_printerr("Unexpected message received.\n");
                break;
        }
        gst_message_unref(msg);
        goto END;
    }
    END:;
}

Streamer::~Streamer() {
    g_printerr("ddddddddddddddddddddddddddddd\n");
    gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}
