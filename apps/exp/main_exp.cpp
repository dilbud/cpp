#include <stdio.h>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/pbutils/pbutils.h>

#define CHUNK_SIZE 1024   /* Amount of bytes we are sending in each buffer */
#define SAMPLE_RATE 44100 /* Samples per second we are sending */

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

int main_thread(int argc, char *argv[]) {
  GstElement *pipeline, *audio_source, *tee, *audio_queue, *audio_convert, *audio_resample, *audio_sink;
  GstElement *video_queue, *visual, *video_convert, *video_sink;
  GstBus *bus;
  GstMessage *msg;
  GstPad *tee_audio_pad, *tee_video_pad;
  GstPad *queue_audio_pad, *queue_video_pad;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Create the elements */
  audio_source = gst_element_factory_make ("audiotestsrc", "audio_source");
  tee = gst_element_factory_make ("tee", "tee");
  audio_queue = gst_element_factory_make ("queue", "audio_queue");
  audio_convert = gst_element_factory_make ("audioconvert", "audio_convert");
  audio_resample = gst_element_factory_make ("audioresample", "audio_resample");
  audio_sink = gst_element_factory_make ("autoaudiosink", "audio_sink");
  video_queue = gst_element_factory_make ("queue", "video_queue");
  visual = gst_element_factory_make ("wavescope", "visual");
  video_convert = gst_element_factory_make ("videoconvert", "csp");
  video_sink = gst_element_factory_make ("autovideosink", "video_sink");

  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("test-pipeline");

  if (!pipeline || !audio_source || !tee || !audio_queue || !audio_convert || !audio_resample || !audio_sink ||
      !video_queue || !visual || !video_convert || !video_sink) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  /* Configure elements */
  g_object_set (audio_source, "freq", 215.0f, NULL);
  g_object_set (visual, "shader", 0, "style", 1, NULL);

  /* Link all elements that can be automatically linked because they have "Always" pads */
  gst_bin_add_many (GST_BIN (pipeline), audio_source, tee, audio_queue, audio_convert, audio_resample, audio_sink,
      video_queue, visual, video_convert, video_sink, NULL);
  if (gst_element_link_many (audio_source, tee, NULL) != TRUE ||
      gst_element_link_many (audio_queue, audio_convert, audio_resample, audio_sink, NULL) != TRUE ||
      gst_element_link_many (video_queue, visual, video_convert, video_sink, NULL) != TRUE) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }
  /* Manually link the Tee, which has "Request" pads */
  tee_audio_pad = gst_element_get_request_pad(tee, "src_%u");
  g_print ("Obtained request pad %s for audio branch.\n", gst_pad_get_name (tee_audio_pad));
  queue_audio_pad = gst_element_get_static_pad (audio_queue, "sink");

  tee_video_pad = gst_element_get_request_pad (tee, "src_%u");
  g_print ("Obtained request pad %s for video branch.\n", gst_pad_get_name (tee_video_pad));
  queue_video_pad = gst_element_get_static_pad (video_queue, "sink");

  if (gst_pad_link (tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
      gst_pad_link (tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK) {
    g_printerr ("Tee could not be linked.\n");
    gst_object_unref (pipeline);
    return -1;
  }
  gst_object_unref (queue_audio_pad);
  gst_object_unref (queue_video_pad);

  /* Start playing the pipeline */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

  /* Release the request pads from the Tee, and unref them */
  gst_element_release_request_pad (tee, tee_audio_pad);
  gst_element_release_request_pad (tee, tee_video_pad);
  gst_object_unref (tee_audio_pad);
  gst_object_unref (tee_video_pad);

  /* Free resources */
  if (msg != NULL)
    gst_message_unref (msg);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);
  return 0;
}

// appsrc appsink
////////////////////////////////////////////////////////////////////

typedef struct _CustomData {
  GstElement *pipeline, *app_source, *tee, *audio_queue, *audio_convert1, *audio_resample, *audio_sink;
  GstElement *video_queue, *audio_convert2, *visual, *video_convert, *video_sink;
  GstElement *app_queue, *app_sink;

  guint64 num_samples;   /* Number of samples generated so far (for timestamp generation) */
  gfloat a, b, c, d;     /* For waveform generation */

  guint sourceid;        /* To control the GSource */

  GMainLoop *main_loop;  /* GLib's Main Loop */
} CustomData;

/* This method is called by the idle GSource in the mainloop, to feed CHUNK_SIZE bytes into appsrc.
 * The idle handler is added to the mainloop when appsrc requests us to start sending data (need-data signal)
 * and is removed when appsrc has enough data (enough-data signal).
 */
static gboolean push_data (CustomData *data) {
  GstBuffer *buffer;
  GstFlowReturn ret;
  int i;
  GstMapInfo map;
  gint16 *raw;
  gint num_samples = CHUNK_SIZE / 2; /* Because each sample is 16 bits */
  gfloat freq;

  /* Create a new empty buffer */
  buffer = gst_buffer_new_and_alloc (CHUNK_SIZE);

  /* Set its timestamp and duration */
  GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale (data->num_samples, GST_SECOND, SAMPLE_RATE);
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (num_samples, GST_SECOND, SAMPLE_RATE);

  /* Generate some psychodelic waveforms */
  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  raw = (gint16 *)map.data;
  data->c += data->d;
  data->d -= data->c / 1000;
  freq = 1100 + 1000 * data->d;
  for (i = 0; i < num_samples; i++) {
    data->a += data->b;
    data->b -= data->a / freq;
    raw[i] = (gint16)(500 * data->a);
  }
  gst_buffer_unmap (buffer, &map);
  data->num_samples += num_samples;

  /* Push the buffer into the appsrc */
  g_signal_emit_by_name (data->app_source, "push-buffer", buffer, &ret);

  /* Free the buffer now that we are done with it */
  gst_buffer_unref (buffer);

  if (ret != GST_FLOW_OK) {
    /* We got some error, stop sending data */
    return FALSE;
  }

  return TRUE;
}

/* This signal callback triggers when appsrc needs data. Here, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void start_feed (GstElement *source, guint size, CustomData *data) {
  if (data->sourceid == 0) {
    g_print ("Start feeding\n");
    data->sourceid = g_idle_add ((GSourceFunc) push_data, data);
  }
}

/* This callback triggers when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void stop_feed (GstElement *source, CustomData *data) {
  if (data->sourceid != 0) {
    g_print ("Stop feeding\n");
    g_source_remove (data->sourceid);
    data->sourceid = 0;
  }
}

/* The appsink has received a buffer */
static GstFlowReturn new_sample (GstElement *sink, CustomData *data) {
  GstSample *sample;

  /* Retrieve the buffer */
  g_signal_emit_by_name (sink, "pull-sample", &sample);
  if (sample) {
    /* The only thing we do in this example is print a * to indicate a received buffer */
    g_print ("*");
    gst_sample_unref (sample);
    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

/* This function is called when an error message is posted on the bus */
static void error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  gst_message_parse_error (msg, &err, &debug_info);
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error (&err);
  g_free (debug_info);

  g_main_loop_quit (data->main_loop);
}

int main_src_sink(int argc, char *argv[]) {
  CustomData data;
  GstPad *tee_audio_pad, *tee_video_pad, *tee_app_pad;
  GstPad *queue_audio_pad, *queue_video_pad, *queue_app_pad;
  GstAudioInfo info;
  GstCaps *audio_caps;
  GstBus *bus;

  /* Initialize custom data structure */
  memset (&data, 0, sizeof (data));
  data.b = 1; /* For waveform generation */
  data.d = 1;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Create the elements */
  data.app_source = gst_element_factory_make ("appsrc", "audio_source");
  data.tee = gst_element_factory_make ("tee", "tee");
  data.audio_queue = gst_element_factory_make ("queue", "audio_queue");
  data.audio_convert1 = gst_element_factory_make ("audioconvert", "audio_convert1");
  data.audio_resample = gst_element_factory_make ("audioresample", "audio_resample");
  data.audio_sink = gst_element_factory_make ("autoaudiosink", "audio_sink");
  data.video_queue = gst_element_factory_make ("queue", "video_queue");
  data.audio_convert2 = gst_element_factory_make ("audioconvert", "audio_convert2");
  data.visual = gst_element_factory_make ("wavescope", "visual");
  data.video_convert = gst_element_factory_make ("videoconvert", "video_convert");
  data.video_sink = gst_element_factory_make ("autovideosink", "video_sink");
  data.app_queue = gst_element_factory_make ("queue", "app_queue");
  data.app_sink = gst_element_factory_make ("appsink", "app_sink");

  /* Create the empty pipeline */
  data.pipeline = gst_pipeline_new ("test-pipeline");

  if (!data.pipeline || !data.app_source || !data.tee || !data.audio_queue || !data.audio_convert1 ||
      !data.audio_resample || !data.audio_sink || !data.video_queue || !data.audio_convert2 || !data.visual ||
      !data.video_convert || !data.video_sink || !data.app_queue || !data.app_sink) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  /* Configure wavescope */
  g_object_set (data.visual, "shader", 0, "style", 0, NULL);

  /* Configure appsrc */
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16, SAMPLE_RATE, 1, NULL);
  audio_caps = gst_audio_info_to_caps (&info);
  g_object_set (data.app_source, "caps", audio_caps, "format", GST_FORMAT_TIME, NULL);
  g_signal_connect (data.app_source, "need-data", G_CALLBACK (start_feed), &data);
  g_signal_connect (data.app_source, "enough-data", G_CALLBACK (stop_feed), &data);

  /* Configure appsink */
  g_object_set (data.app_sink, "emit-signals", TRUE, "caps", audio_caps, NULL);
  g_signal_connect (data.app_sink, "new-sample", G_CALLBACK (new_sample), &data);
  gst_caps_unref (audio_caps);

  /* Link all elements that can be automatically linked because they have "Always" pads */
  gst_bin_add_many (GST_BIN (data.pipeline), data.app_source, data.tee, data.audio_queue, data.audio_convert1, data.audio_resample,
      data.audio_sink, data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, data.app_queue,
      data.app_sink, NULL);
  if (gst_element_link_many (data.app_source, data.tee, NULL) != TRUE ||
      gst_element_link_many (data.audio_queue, data.audio_convert1, data.audio_resample, data.audio_sink, NULL) != TRUE ||
      gst_element_link_many (data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, NULL) != TRUE ||
      gst_element_link_many (data.app_queue, data.app_sink, NULL) != TRUE) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }

  /* Manually link the Tee, which has "Request" pads */
  tee_audio_pad = gst_element_get_request_pad(data.tee, "src_%u");
  g_print ("Obtained request pad %s for audio branch.\n", gst_pad_get_name (tee_audio_pad));
  queue_audio_pad = gst_element_get_static_pad (data.audio_queue, "sink");
  tee_video_pad = gst_element_get_request_pad(data.tee, "src_%u");
  g_print ("Obtained request pad %s for video branch.\n", gst_pad_get_name (tee_video_pad));
  queue_video_pad = gst_element_get_static_pad (data.video_queue, "sink");
  tee_app_pad = gst_element_get_request_pad(data.tee, "src_%u");
  g_print ("Obtained request pad %s for app branch.\n", gst_pad_get_name (tee_app_pad));
  queue_app_pad = gst_element_get_static_pad (data.app_queue, "sink");
  if (gst_pad_link (tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
      gst_pad_link (tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK ||
      gst_pad_link (tee_app_pad, queue_app_pad) != GST_PAD_LINK_OK) {
    g_printerr ("Tee could not be linked\n");
    gst_object_unref (data.pipeline);
    return -1;
  }
  gst_object_unref (queue_audio_pad);
  gst_object_unref (queue_video_pad);
  gst_object_unref (queue_app_pad);

  /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  bus = gst_element_get_bus (data.pipeline);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, &data);
  gst_object_unref (bus);

  /* Start playing the pipeline */
  gst_element_set_state (data.pipeline, GST_STATE_PLAYING);

  /* Create a GLib Main Loop and set it to run */
  data.main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (data.main_loop);

  /* Release the request pads from the Tee, and unref them */
  gst_element_release_request_pad (data.tee, tee_audio_pad);
  gst_element_release_request_pad (data.tee, tee_video_pad);
  gst_element_release_request_pad (data.tee, tee_app_pad);
  gst_object_unref (tee_audio_pad);
  gst_object_unref (tee_video_pad);
  gst_object_unref (tee_app_pad);

  /* Free resources */
  gst_element_set_state (data.pipeline, GST_STATE_NULL);
  gst_object_unref (data.pipeline);
  return 0;
}


////////////////////////////////////////////////////////////////////


typedef struct _CustomDataInfo {
  GstDiscoverer *discoverer;
  GMainLoop *loop;
} CustomDataInfo;

/* Print a tag in a human-readable format (name: value) */
static void print_tag_foreach (const GstTagList *tags, const gchar *tag, gpointer user_data) {
  GValue val = { 0, };
  gchar *str;
  gint depth = GPOINTER_TO_INT (user_data);

  gst_tag_list_copy_value (&val, tags, tag);

  if (G_VALUE_HOLDS_STRING (&val))
    str = g_value_dup_string (&val);
  else
    str = gst_value_serialize (&val);

  g_print ("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag), str);
  g_free (str);

  g_value_unset (&val);
}

/* Print information regarding a stream */
static void print_stream_info (GstDiscovererStreamInfo *info, gint depth) {
  gchar *desc = NULL;
  GstCaps *caps;
  const GstTagList *tags;

  caps = gst_discoverer_stream_info_get_caps (info);

  if (caps) {
    if (gst_caps_is_fixed (caps))
      desc = gst_pb_utils_get_codec_description (caps);
    else
      desc = gst_caps_to_string (caps);
    gst_caps_unref (caps);
  }

  g_print ("%*s%s: %s\n", 2 * depth, " ", gst_discoverer_stream_info_get_stream_type_nick (info), (desc ? desc : ""));

  if (desc) {
    g_free (desc);
    desc = NULL;
  }

  tags = gst_discoverer_stream_info_get_tags (info);
  if (tags) {
    g_print ("%*sTags:\n", 2 * (depth + 1), " ");
    gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER (depth + 2));
  }
}

/* Print information regarding a stream and its substreams, if any */
static void print_topology (GstDiscovererStreamInfo *info, gint depth) {
  GstDiscovererStreamInfo *next;

  if (!info)
    return;

  print_stream_info (info, depth);

  next = gst_discoverer_stream_info_get_next (info);
  if (next) {
    print_topology (next, depth + 1);
    gst_discoverer_stream_info_unref (next);
  } else if (GST_IS_DISCOVERER_CONTAINER_INFO (info)) {
    GList *tmp, *streams;

    streams = gst_discoverer_container_info_get_streams (GST_DISCOVERER_CONTAINER_INFO (info));
    for (tmp = streams; tmp; tmp = tmp->next) {
      GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
      print_topology (tmpinf, depth + 1);
    }
    gst_discoverer_stream_info_list_free (streams);
  }
}

/* This function is called every time the discoverer has information regarding
 * one of the URIs we provided.*/
static void on_discovered_cb (GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, CustomData *data) {
  GstDiscovererResult result;
  const gchar *uri;
  const GstTagList *tags;
  GstDiscovererStreamInfo *sinfo;

  uri = gst_discoverer_info_get_uri (info);
  result = gst_discoverer_info_get_result (info);
  switch (result) {
    case GST_DISCOVERER_URI_INVALID:
      g_print ("Invalid URI '%s'\n", uri);
      break;
    case GST_DISCOVERER_ERROR:
      g_print ("Discoverer error: %s\n", err->message);
      break;
    case GST_DISCOVERER_TIMEOUT:
      g_print ("Timeout\n");
      break;
    case GST_DISCOVERER_BUSY:
      g_print ("Busy\n");
      break;
    case GST_DISCOVERER_MISSING_PLUGINS:{
      const GstStructure *s;
      gchar *str;

      s = gst_discoverer_info_get_misc (info);
      str = gst_structure_to_string (s);

      g_print ("Missing plugins: %s\n", str);
      g_free (str);
      break;
    }
    case GST_DISCOVERER_OK:
      g_print ("Discovered '%s'\n", uri);
      break;
  }

  if (result != GST_DISCOVERER_OK) {
    g_printerr ("This URI cannot be played\n");
    return;
  }

  /* If we got no error, show the retrieved information */

  g_print ("\nDuration: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS (gst_discoverer_info_get_duration (info)));

  tags = gst_discoverer_info_get_tags (info);
  if (tags) {
    g_print ("Tags:\n");
    gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER (1));
  }

  g_print ("Seekable: %s\n", (gst_discoverer_info_get_seekable (info) ? "yes" : "no"));

  g_print ("\n");

  sinfo = gst_discoverer_info_get_stream_info (info);
  if (!sinfo)
    return;

  g_print ("Stream information:\n");

  print_topology (sinfo, 1);

  gst_discoverer_stream_info_unref (sinfo);

  g_print ("\n");
}

/* This function is called when the discoverer has finished examining
 * all the URIs we provided.*/
static void on_finished_cb (GstDiscoverer *discoverer, CustomDataInfo *data) {
  g_print ("Finished discovering\n");

  g_main_loop_quit (data->loop);
}

int main_info (int argc, char *argv[]) {
  CustomDataInfo data;
  GError *err = NULL;
  gchar *uri = "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm";

  /* if a URI was provided, use it instead of the default one */
  if (argc > 1) {
    uri = argv[1];
  }

  /* Initialize custom data structure */
  memset (&data, 0, sizeof (data));

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  g_print ("Discovering '%s'\n", uri);

  /* Instantiate the Discoverer */
  data.discoverer = gst_discoverer_new (5 * GST_SECOND, &err);
  if (!data.discoverer) {
    g_print ("Error creating discoverer instance: %s\n", err->message);
    g_clear_error (&err);
    return -1;
  }

  /* Connect to the interesting signals */
  g_signal_connect (data.discoverer, "discovered", G_CALLBACK (on_discovered_cb), &data);
  g_signal_connect (data.discoverer, "finished", G_CALLBACK (on_finished_cb), &data);

  /* Start the discoverer process (nothing to do yet) */
  gst_discoverer_start (data.discoverer);

  /* Add a request to process asynchronously the URI passed through the command line */
  if (!gst_discoverer_discover_uri_async (data.discoverer, uri)) {
    g_print ("Failed to start discovering URI '%s'\n", uri);
    g_object_unref (data.discoverer);
    return -1;
  }

  /* Create a GLib Main Loop and set it to run, so we can wait for the signals */
  data.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (data.loop);

  /* Stop the discoverer process */
  gst_discoverer_stop (data.discoverer);

  /* Free resources */
  g_object_unref (data.discoverer);
  g_main_loop_unref (data.loop);

  return 0;
}

////////////////////////////////////////////////////////////////////

typedef struct _CustomDataStr {
  gboolean is_live;
  GstElement *pipeline;
  GMainLoop *loop;
} CustomDataStr;

static void cb_message (GstBus *bus, GstMessage *msg, CustomDataStr *data) {

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR: {
      GError *err;
      gchar *debug;

      gst_message_parse_error (msg, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_main_loop_quit (data->loop);
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_main_loop_quit (data->loop);
      break;
    case GST_MESSAGE_BUFFERING: {
      gint percent = 0;

      /* If the stream is live, we do not care about buffering. */
      if (data->is_live) break;

      gst_message_parse_buffering (msg, &percent);
      g_print ("Buffering (%3d%%)\r", percent);
      /* Wait until buffering is complete before start/resume playing */
      if (percent < 100)
        gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
      else
        gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
      break;
    }
    case GST_MESSAGE_CLOCK_LOST:
      /* Get a new clock */
      gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
      gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
      break;
    default:
      /* Unhandled message */
      break;
    }
}

int main_stream(int argc, char *argv[]) {
  GstElement *pipeline;
  GstBus *bus;
  GstStateChangeReturn ret;
  GMainLoop *main_loop;
  CustomDataStr data;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Initialize our data structure */
  memset (&data, 0, sizeof (data));

  /* Build the pipeline */
  pipeline = gst_parse_launch ("playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", NULL);
  bus = gst_element_get_bus (pipeline);

  /* Start playing */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    return -1;
  } else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
    data.is_live = TRUE;
  }

  main_loop = g_main_loop_new (NULL, FALSE);
  data.loop = main_loop;
  data.pipeline = pipeline;

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (cb_message), &data);

  g_main_loop_run (main_loop);

  /* Free resources */
  g_main_loop_unref (main_loop);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}

////////////////////////////////////////////////////////////////////

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

typedef struct _CustomDataSpeed
{
  GstElement *pipeline;
  GstElement *video_sink;
  GMainLoop *loop;

  gboolean playing;             /* Playing or Paused */
  gdouble rate;                 /* Current playback rate (can be negative) */
} CustomDataSpeed;

/* Send seek event to change rate */
static void
send_seek_event (CustomDataSpeed * data)
{
  gint64 position;
  GstEvent *seek_event;

  /* Obtain the current position, needed for the seek event */
  if (!gst_element_query_position (data->pipeline, GST_FORMAT_TIME, &position)) {
    g_printerr ("Unable to retrieve current position.\n");
    return;
  }

  /* Create the seek event */
  if (data->rate > 0) {
    seek_event =
        gst_event_new_seek (data->rate,
                            GST_FORMAT_TIME,
                            GstSeekFlags (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                            GST_SEEK_TYPE_SET,
                            position, 
                            GST_SEEK_TYPE_END,
                            0);
  } else {
    seek_event =
        gst_event_new_seek (data->rate,
                            GST_FORMAT_TIME,
                            GstSeekFlags (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), 
                            GST_SEEK_TYPE_SET,
                            0,
                            GST_SEEK_TYPE_SET, 
                            position);
  }

  if (data->video_sink == NULL) {
    /* If we have not done so, obtain the sink through which we will send the seek events */
    g_object_get (data->pipeline, "video-sink", &data->video_sink, NULL);
  }

  /* Send the event */
  gst_element_send_event (data->video_sink, seek_event);

  g_print ("Current rate: %g\n", data->rate);
}

/* Process keyboard input */
static gboolean
handle_keyboard (GIOChannel * source, GIOCondition cond, CustomDataSpeed * data)
{
  gchar *str = NULL;

  if (g_io_channel_read_line (source, &str, NULL, NULL,
          NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }

  switch (g_ascii_tolower (str[0])) {
    case 'p':
      data->playing = !data->playing;
      gst_element_set_state (data->pipeline,
          data->playing ? GST_STATE_PLAYING : GST_STATE_PAUSED);
      g_print ("Setting state to %s\n", data->playing ? "PLAYING" : "PAUSE");
      break;
    case 's':
      if (g_ascii_isupper (str[0])) {
        data->rate *= 2.0;
      } else {
        data->rate /= 2.0;
      }
      send_seek_event (data);
      break;
    case 'd':
      data->rate *= -1.0;
      send_seek_event (data);
      break;
    case 'n':
      if (data->video_sink == NULL) {
        /* If we have not done so, obtain the sink through which we will send the step events */
        g_object_get (data->pipeline, "video-sink", &data->video_sink, NULL);
      }

      gst_element_send_event (data->video_sink,
          gst_event_new_step (GST_FORMAT_BUFFERS, 1, ABS (data->rate), TRUE,
              FALSE));
      g_print ("Stepping one frame\n");
      break;
    case 'q':
      g_main_loop_quit (data->loop);
      break;
    default:
      break;
  }

  g_free (str);

  return TRUE;
}

int
tutorial_main (int argc, char *argv[])
{
  CustomDataSpeed data;
  GstStateChangeReturn ret;
  GIOChannel *io_stdin;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Initialize our data structure */
  memset (&data, 0, sizeof (data));

  /* Print usage map */
  g_print ("USAGE: Choose one of the following options, then press enter:\n"
      " 'P' to toggle between PAUSE and PLAY\n"
      " 'S' to increase playback speed, 's' to decrease playback speed\n"
      " 'D' to toggle playback direction\n"
      " 'N' to move to next frame (in the current direction, better in PAUSE)\n"
      " 'Q' to quit\n");

  /* Build the pipeline */
  data.pipeline =
      gst_parse_launch
      ("playbin uri=https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
      NULL);

  /* Add a keyboard watch so we get notified of keystrokes */
#ifdef G_OS_WIN32
  io_stdin = g_io_channel_win32_new_fd (fileno (stdin));
#else
  io_stdin = g_io_channel_unix_new (fileno (stdin));
#endif
  g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc) handle_keyboard, &data);

  /* Start playing */
  ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }
  data.playing = TRUE;
  data.rate = 1.0;

  /* Create a GLib Main Loop and set it to run */
  data.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (data.loop);

  /* Free resources */
  g_main_loop_unref (data.loop);
  g_io_channel_unref (io_stdin);
  gst_element_set_state (data.pipeline, GST_STATE_NULL);
  if (data.video_sink != NULL)
    gst_object_unref (data.video_sink);
  gst_object_unref (data.pipeline);
  return 0;
}

int main_speed (int argc, char *argv[])
{
#if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
  return gst_macos_main (tutorial_main, argc, argv, NULL);
#else
  return tutorial_main (argc, argv);
#endif
}


////////////////////////////////////////////////////////////////////


/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomDataMultiLang {
  GstElement *playbin;  /* Our one and only element */

  gint n_video;          /* Number of embedded video streams */
  gint n_audio;          /* Number of embedded audio streams */
  gint n_text;           /* Number of embedded subtitle streams */

  gint current_video;    /* Currently playing video stream */
  gint current_audio;    /* Currently playing audio stream */
  gint current_text;     /* Currently playing subtitle stream */

  GMainLoop *main_loop;  /* GLib's Main Loop */
} CustomDataMultiLang;

/* playbin flags */
typedef enum {
  GST_PLAY_FLAG_VIDEO         = (1 << 0), /* We want video output */
  GST_PLAY_FLAG_AUDIO         = (1 << 1), /* We want audio output */
  GST_PLAY_FLAG_TEXT          = (1 << 2)  /* We want subtitle output */
} GstPlayFlags;

/* Forward definition for the message and keyboard processing functions */
static gboolean handle_messageMultiLang (GstBus *bus, GstMessage *msg, CustomDataMultiLang *data);
static gboolean handle_keyboardMultiLang (GIOChannel *source, GIOCondition cond, CustomDataMultiLang *data);

int main_MultiLang(int argc, char *argv[]) {
  CustomDataMultiLang data;
  GstBus *bus;
  GstStateChangeReturn ret;
  gint flags;
  GIOChannel *io_stdin;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Create the elements */
  data.playbin = gst_element_factory_make ("playbin", "playbin");

  if (!data.playbin) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  /* Set the URI to play */
  g_object_set (data.playbin, "uri", "https://gstreamer.freedesktop.org/data/media/sintel_cropped_multilingual.webm", NULL);

  /* Set flags to show Audio and Video but ignore Subtitles */
  g_object_get (data.playbin, "flags", &flags, NULL);
  flags |= GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO;
  flags &= ~GST_PLAY_FLAG_TEXT;
  g_object_set (data.playbin, "flags", flags, NULL);

  /* Set connection speed. This will affect some internal decisions of playbin */
  g_object_set (data.playbin, "connection-speed", 56, NULL);

  /* Add a bus watch, so we get notified when a message arrives */
  bus = gst_element_get_bus (data.playbin);
  gst_bus_add_watch (bus, (GstBusFunc)handle_messageMultiLang, &data);

  /* Add a keyboard watch so we get notified of keystrokes */
#ifdef G_OS_WIN32
  io_stdin = g_io_channel_win32_new_fd (fileno (stdin));
#else
  io_stdin = g_io_channel_unix_new (fileno (stdin));
#endif
  g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc)handle_keyboardMultiLang, &data);

  /* Start playing */
  ret = gst_element_set_state (data.playbin, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (data.playbin);
    return -1;
  }

  /* Create a GLib Main Loop and set it to run */
  data.main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (data.main_loop);

  /* Free resources */
  g_main_loop_unref (data.main_loop);
  g_io_channel_unref (io_stdin);
  gst_object_unref (bus);
  gst_element_set_state (data.playbin, GST_STATE_NULL);
  gst_object_unref (data.playbin);
  return 0;
}

/* Extract some metadata from the streams and print it on the screen */
static void analyze_streams (CustomDataMultiLang *data) {
  gint i;
  GstTagList *tags;
  gchar *str;
  guint rate;

  /* Read some properties */
  g_object_get (data->playbin, "n-video", &data->n_video, NULL);
  g_object_get (data->playbin, "n-audio", &data->n_audio, NULL);
  g_object_get (data->playbin, "n-text", &data->n_text, NULL);

  g_print ("%d video stream(s), %d audio stream(s), %d text stream(s)\n",
    data->n_video, data->n_audio, data->n_text);

  g_print ("\n");
  for (i = 0; i < data->n_video; i++) {
    tags = NULL;
    /* Retrieve the stream's video tags */
    g_signal_emit_by_name (data->playbin, "get-video-tags", i, &tags);
    if (tags) {
      g_print ("video stream %d:\n", i);
      gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &str);
      g_print ("  codec: %s\n", str ? str : "unknown");
      g_free (str);
      gst_tag_list_free (tags);
    }
  }

  g_print ("\n");
  for (i = 0; i < data->n_audio; i++) {
    tags = NULL;
    /* Retrieve the stream's audio tags */
    g_signal_emit_by_name (data->playbin, "get-audio-tags", i, &tags);
    if (tags) {
      g_print ("audio stream %d:\n", i);
      if (gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &str)) {
        g_print ("  codec: %s\n", str);
        g_free (str);
      }
      if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
        g_print ("  language: %s\n", str);
        g_free (str);
      }
      if (gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &rate)) {
        g_print ("  bitrate: %d\n", rate);
      }
      gst_tag_list_free (tags);
    }
  }

  g_print ("\n");
  for (i = 0; i < data->n_text; i++) {
    tags = NULL;
    /* Retrieve the stream's subtitle tags */
    g_signal_emit_by_name (data->playbin, "get-text-tags", i, &tags);
    if (tags) {
      g_print ("subtitle stream %d:\n", i);
      if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
        g_print ("  language: %s\n", str);
        g_free (str);
      }
      gst_tag_list_free (tags);
    }
  }

  g_object_get (data->playbin, "current-video", &data->current_video, NULL);
  g_object_get (data->playbin, "current-audio", &data->current_audio, NULL);
  g_object_get (data->playbin, "current-text", &data->current_text, NULL);

  g_print ("\n");
  g_print ("Currently playing video stream %d, audio stream %d and text stream %d\n",
    data->current_video, data->current_audio, data->current_text);
  g_print ("Type any number and hit ENTER to select a different audio stream\n");
}

/* Process messages from GStreamer */
static gboolean handle_messageMultiLang (GstBus *bus, GstMessage *msg, CustomDataMultiLang *data) {
  GError *err;
  gchar *debug_info;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (msg, &err, &debug_info);
      g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
      g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
      g_clear_error (&err);
      g_free (debug_info);
      g_main_loop_quit (data->main_loop);
      break;
    case GST_MESSAGE_EOS:
      g_print ("End-Of-Stream reached.\n");
      g_main_loop_quit (data->main_loop);
      break;
    case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state, pending_state;
      gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
      if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
        if (new_state == GST_STATE_PLAYING) {
          /* Once we are in the playing state, analyze the streams */
          analyze_streams (data);
        }
      }
    } break;
  }

  /* We want to keep receiving messages */
  return TRUE;
}

/* Process keyboard input */
static gboolean handle_keyboardMultiLang (GIOChannel *source, GIOCondition cond, CustomDataMultiLang *data) {
  gchar *str = NULL;

  if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
    int index = g_ascii_strtoull (str, NULL, 0);
    if (index < 0 || index >= data->n_audio) {
      g_printerr ("Index out of bounds\n");
    } else {
      /* If the input was a valid audio stream index, set the current audio stream */
      g_print ("Setting current audio stream to %d\n", index);
      g_object_set (data->playbin, "current-audio", index, NULL);
    }
  }
  g_free (str);
  return TRUE;
}

////////////////////////////////////////////////////////////////////

int main_Subtitle(int argc, char *argv[]) {
  
}


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
    // main_thread(argc, argv);
    // Streamer stm(argc, argv);
    // main_src_sink(argc, argv);
    // main_info(argc, argv);
    // main_stream(argc, argv);
    // main_speed(argc, argv);
    // main_MultiLang(argc, argv);
    main_Subtitle(argc, argv);
    return 0;
}