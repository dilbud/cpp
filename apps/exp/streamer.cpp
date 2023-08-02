#include "streamer.h"

void Streamer::handle_message(Streamer *data, GstMessage *msg) {
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
            data->terminate = TRUE;
            break;
        case GST_MESSAGE_EOS:
            g_print("\nEnd-Of-Stream reached.\n");
            data->terminate = TRUE;
            break;
        case GST_MESSAGE_DURATION:
            /* The duration has changed, mark the current one as invalid */
            data->duration = GST_CLOCK_TIME_NONE;
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state,
                                            &pending_state);
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline)) {
                g_print("Pipeline state changed from %s to %s:\n",
                        gst_element_state_get_name(old_state),
                        gst_element_state_get_name(new_state));

                /* Remember whether we are in the PLAYING state or not */
                data->playing = (new_state == GST_STATE_PLAYING);

                if (data->playing) {
                    /* We just moved to PLAYING. Check if seeking is possible */
                    GstQuery *query;
                    gint64 start, end;
                    query = gst_query_new_seeking(GST_FORMAT_TIME);
                    if (gst_element_query(data->pipeline, query)) {
                        gst_query_parse_seeking(
                            query, NULL, &data->seek_enabled, &start, &end);
                        if (data->seek_enabled) {
                            g_print("Seeking is ENABLED from %" GST_TIME_FORMAT
                                    " to %" GST_TIME_FORMAT "\n",
                                    GST_TIME_ARGS(start), GST_TIME_ARGS(end));
                        } else {
                            g_print("Seeking is DISABLED for this stream.\n");
                        }
                    } else {
                        g_printerr("Seeking query failed.");
                    }
                    gst_query_unref(query);
                }
            }
        } break;
        default:
            /* We should not reach here */
            g_printerr("Unexpected message received.\n");
            break;
    }
    gst_message_unref(msg);
}

void Streamer::pad_added_handler(GstElement *src, GstPad *new_pad,
                                 Streamer *data) {
    GstPad *sink_pad = gst_element_get_static_pad(data->convert01, "sink");
    GstPad *a_sink_pad = gst_element_get_static_pad(data->a_convert, "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad),
            GST_ELEMENT_NAME(src));

    /* If our converter is already linked, we have nothing to do here */
    if (gst_pad_is_linked(sink_pad) && gst_pad_is_linked(a_sink_pad)) {
        g_print("We are already linked. Ignoring.\n");
        goto exit;
    }

    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    g_print("Type is '%s'\n", new_pad_type);

    if (g_str_has_prefix(new_pad_type, "video/x-raw") &&
        !gst_pad_is_linked(sink_pad)) {
        ret = gst_pad_link(new_pad, sink_pad);
        if (GST_PAD_LINK_FAILED(ret)) {
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        } else {
            g_print("Link succeeded (type '%s').\n", new_pad_type);
        }
    } else if (g_str_has_prefix(new_pad_type, "audio/x-raw") &&
               !gst_pad_is_linked(a_sink_pad)) {
        ret = gst_pad_link(new_pad, a_sink_pad);
        if (GST_PAD_LINK_FAILED(ret)) {
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        } else {
            g_print("Link succeeded (type '%s').\n", new_pad_type);
        }
    }

    /* Attempt the link */

exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL) gst_caps_unref(new_pad_caps);

    /* Unreference the sink pad */
    gst_object_unref(sink_pad);
    gst_object_unref(a_sink_pad);
}

Streamer::Streamer(int argc, char *argv[]) {
    GstStateChangeReturn ret;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the elements */
    source = gst_element_factory_make("uridecodebin", "source");
    // gst_element_factory_make("videotestsrc", "source01");

    // video
    convert01 = gst_element_factory_make("videoconvert", "convert01");
    filter = gst_element_factory_make(
        // "vertigotv", 
        "navseek",
        "filter01");
    convert02 = gst_element_factory_make("videoconvert", "convert02");
    sink = gst_element_factory_make("autovideosink", "sink01");

    // audio
    a_convert = gst_element_factory_make("audioconvert", "convert");
    a_resample = gst_element_factory_make("audioresample", "resample");
    a_sink = gst_element_factory_make("autoaudiosink", "sink");

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("pipe");

    if (!pipeline || !source || !filter || !convert01 || !convert02 || !sink) {
        g_printerr("Not all elements could be created.\n");
        goto END;
    }

    if (!a_convert || !a_resample || !a_sink) {
        g_printerr("Not all 2 elements could be created.\n");
        goto END;
    }

    /* Build the pipeline */
    gst_bin_add_many(GST_BIN(pipeline), source, convert01, filter, convert02,
                     sink, NULL);
    gst_bin_add_many(GST_BIN(pipeline), a_convert, a_resample, a_sink, NULL);

    // if (gst_element_link(source, convert01) != TRUE) {
    //     g_printerr("Elements 01 could not be linked.\n");
    //     gst_object_unref(pipeline);
    //     goto END;
    // }
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

    if (gst_element_link(a_convert, a_resample) != TRUE) {
        g_printerr("Elements 05 could not be linked.\n");
        gst_object_unref(pipeline);
        goto END;
    }

    if (gst_element_link(a_resample, a_sink) != TRUE) {
        g_printerr("Elements 06 could not be linked.\n");
        gst_object_unref(pipeline);
        goto END;
    }

    /* Modify the source's properties */
    g_object_set(source, "uri",
                 "file:///workspaces/cpp/video_data/sintel-short.mp4", NULL);
    g_signal_connect(source, "pad-added", G_CALLBACK(pad_added_handler), this);

    /* Start playing */
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        goto END;
    }

    /* Wait until error or EOS */
    bus = gst_element_get_bus(pipeline);
    do {
        msg = gst_bus_timed_pop_filtered(
            bus, 100 * GST_MSECOND,
            GstMessageType(GST_MESSAGE_STATE_CHANGED |GST_MESSAGE_ERROR |
                           GST_MESSAGE_EOS | GST_MESSAGE_DURATION));
        /* Parse message */
        if (msg != NULL) {
            handle_message(this, msg);
        } else {
            /* We got no message, this means the timeout expired */
            if (this->playing) {
                gint64 current = -1;

                /* Query the current position of the stream */
                if (!gst_element_query_position(this->pipeline, GST_FORMAT_TIME,
                                                &current)) {
                    g_printerr("Could not query current position.\n");
                }

                /* If we didn't know it yet, query the stream duration */
                if (!GST_CLOCK_TIME_IS_VALID(this->duration)) {
                    if (!gst_element_query_duration(this->pipeline,
                                                    GST_FORMAT_TIME,
                                                    &(this->duration))) {
                        g_printerr("Could not query current duration.\n");
                    }
                }

                /* Print current position and total duration */
                g_print("Position %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT
                        "\r",
                        GST_TIME_ARGS(current), GST_TIME_ARGS(this->duration));

                /* If seeking is enabled, we have not done it yet, and the time
                 * is right, seek */
                if (this->seek_enabled && !(this->seek_done) &&
                    current > 10 * GST_SECOND) {
                    g_print("\nReached 10s, performing seek...\n");
                    gst_element_seek_simple(
                        this->pipeline, GST_FORMAT_TIME,
                        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH |
                                       GST_SEEK_FLAG_KEY_UNIT),
                        90 * GST_SECOND);
                    this->seek_done = TRUE;
                }
            }
        }

    } while (!terminate);
END:;
}

Streamer::~Streamer() {
    g_printerr("ddddddddddddddddddddddddddddd\n");
    gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}
