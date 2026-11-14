#pragma once

#include <cstring>
#include "AudioProcessor.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <iostream>
#include <string>

class AudioPipeline {
public:
    explicit AudioPipeline(float gain)
        : processor_(gain) {}

    bool play(const std::string& uri) {
        GError* error = nullptr;

        // Decode input into 32-bit floating-point PCM
        GstElement* input = gst_element_factory_make(
            "uridecodebin", "decoder"
        );

        GstElement* converter = gst_element_factory_make(
            "audioconvert", "converter"
        );

        GstElement* resampler = gst_element_factory_make(
            "audioresample", "resampler"
        );

        GstElement* sink = gst_element_factory_make(
            "appsink", "input-sink"
        );

        GstElement* output = gst_parse_launch(
            "appsrc name=output-source is-live=false format=time "
            "! audioconvert "
            "! audioresample "
            "! autoaudiosink",
            &error
        );

        if (!input || !converter || !resampler ||
            !sink || !output) {
            std::cerr << "Could not create audio pipeline.\n";

            if (error) {
                std::cerr << error->message << "\n";
                g_error_free(error);
            }

            if (input) gst_object_unref(input);
            if (converter) gst_object_unref(converter);
            if (resampler) gst_object_unref(resampler);
            if (sink) gst_object_unref(sink);
            if (output) gst_object_unref(output);

            return false;
        }

        // Request interleaved Float32 PCM
        GstCaps* caps = gst_caps_from_string(
            "audio/x-raw,format=F32LE,layout=interleaved"
        );

        g_object_set(sink, "caps", caps, "sync", FALSE, nullptr);
        gst_caps_unref(caps);

        // Configure the decoder
        g_object_set(input, "uri", uri.c_str(), nullptr);

        // Build the input pipeline
        GstElement* input_pipeline = gst_pipeline_new("input-pipeline");

        gst_bin_add_many(
            GST_BIN(input_pipeline),
            input, converter, resampler, sink,
            nullptr
        );

        if (!gst_element_link_many(converter, resampler, sink, nullptr)) {
            std::cerr << "Could not link input pipeline.\n";
            gst_object_unref(input_pipeline);
            gst_object_unref(output);
            return false;
        }

        // uridecodebin creates its output pad dynamically
        g_signal_connect(
            input,
            "pad-added",
            G_CALLBACK(onPadAdded),
            converter
        );

        GstElement* source = gst_bin_get_by_name(
            GST_BIN(output), "output-source"
        );

        if (!source) {
            std::cerr << "Could not find output source.\n";
            gst_object_unref(input_pipeline);
            gst_object_unref(output);
            return false;
        }

        // Start the input pipeline
        if (gst_element_set_state(input_pipeline, GST_STATE_PLAYING)
            == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Could not start input pipeline.\n";
            cleanup(input_pipeline, output, source);
            return false;
        }

        bool success = true;
        bool output_started = false;

        while (true) {
            GstSample* sample = gst_app_sink_try_pull_sample(
                GST_APP_SINK(sink),
                100 * GST_MSECOND
            );

            if (!sample) {
                GstBus* bus = gst_element_get_bus(input_pipeline);
                GstMessage* message = gst_bus_pop_filtered(
                    bus, GST_MESSAGE_ERROR
                );

                if (message) {
                    printError(message);
                    gst_message_unref(message);
                    success = false;
                }

                gst_object_unref(bus);

                if (!success || gst_app_sink_is_eos(GST_APP_SINK(sink))) {
                    break;
                }

                continue;
            }

            GstBuffer* buffer = gst_sample_get_buffer(sample);
            GstCaps* sample_caps = gst_sample_get_caps(sample);

            // Configure output using the actual decoded audio format
            if (!output_started) {
                g_object_set(source, "caps", sample_caps, nullptr);

                if (gst_element_set_state(output, GST_STATE_PLAYING)
                    == GST_STATE_CHANGE_FAILURE) {
                    std::cerr << "Could not start output pipeline.\n";
                    gst_sample_unref(sample);
                    success = false;
                    break;
                }

                output_started = true;
            }

            GstMapInfo map;

            if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                // Create a writable copy
                GstBuffer* processed = gst_buffer_new_allocate(
                    nullptr, map.size, nullptr
                );

                if (!processed) {
                    std::cerr << "Could not allocate audio buffer.\n";
                    gst_buffer_unmap(buffer, &map);
                    gst_sample_unref(sample);
                    success = false;
                    break;
                }

                GstMapInfo output_map;

                if (!gst_buffer_map(processed, &output_map, GST_MAP_WRITE)) {
                    std::cerr << "Could not map output buffer.\n";
                    gst_buffer_unref(processed);
                    gst_buffer_unmap(buffer, &map);
                    gst_sample_unref(sample);
                    success = false;
                    break;
                }

                std::memcpy(output_map.data, map.data, map.size);

                float* samples = reinterpret_cast<float*>(output_map.data);
                int count = static_cast<int>(
                    output_map.size / sizeof(float)
                );

                // Apply our own C++ DSP
                processor_.process(samples, count);

                gst_buffer_unmap(processed, &output_map);

                GST_BUFFER_PTS(processed) = GST_BUFFER_PTS(buffer);
                GST_BUFFER_DURATION(processed) = GST_BUFFER_DURATION(buffer);

                GstFlowReturn result = gst_app_src_push_buffer(
                    GST_APP_SRC(source), processed
                );

                gst_buffer_unmap(buffer, &map);

                if (result != GST_FLOW_OK) {
                    std::cerr << "Could not push processed audio.\n";
                    gst_sample_unref(sample);
                    success = false;
                    break;
                }
            }

            gst_sample_unref(sample);
        }

        if (output_started) {
            gst_app_src_end_of_stream(GST_APP_SRC(source));

            // Wait until the processed audio has finished playing
            GstBus* bus = gst_element_get_bus(output);
            GstMessage* message = gst_bus_timed_pop_filtered(
                bus,
                10 * GST_SECOND,
                static_cast<GstMessageType>(
                    GST_MESSAGE_EOS | GST_MESSAGE_ERROR
                )
            );

            if (message) {
                if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
                    printError(message);
                    success = false;
                }
                gst_message_unref(message);
            } else {
                std::cerr << "Timed out waiting for output playback.\n";
                success = false;
            }

            gst_object_unref(bus);
        } else {
            std::cerr << "No audio samples were decoded.\n";
            success = false;
        }

        cleanup(input_pipeline, output, source);
        return success;
    }

private:
    AudioProcessor processor_;

    static void onPadAdded(
        GstElement* decoder,
        GstPad* pad,
        gpointer user_data
    ) {
        GstElement* converter = GST_ELEMENT(user_data);
        GstPad* sink_pad = gst_element_get_static_pad(converter, "sink");

        GstCaps* caps = gst_pad_get_current_caps(pad);

        if (!caps) {
            caps = gst_pad_query_caps(pad, nullptr);
        }

        if (caps) {
            const GstStructure* structure = gst_caps_get_structure(caps, 0);
            const char* name = gst_structure_get_name(structure);

            if (g_str_has_prefix(name, "audio/x-raw") &&
                !gst_pad_is_linked(sink_pad)) {
                gst_pad_link(pad, sink_pad);
            }

            gst_caps_unref(caps);
        }

        gst_object_unref(sink_pad);
    }

    static void printError(GstMessage* message) {
        GError* error = nullptr;
        gchar* debug = nullptr;

        gst_message_parse_error(message, &error, &debug);

        std::cerr << "GStreamer error: "
                  << (error ? error->message : "Unknown")
                  << "\n";

        if (debug) {
            std::cerr << "Debug details: " << debug << "\n";
        }

        g_clear_error(&error);
        g_free(debug);
    }

    static void cleanup(
        GstElement* input,
        GstElement* output,
        GstElement* source
    ) {
        gst_element_set_state(input, GST_STATE_NULL);
        gst_element_set_state(output, GST_STATE_NULL);

        gst_object_unref(source);
        gst_object_unref(input);
        gst_object_unref(output);
    }
};
