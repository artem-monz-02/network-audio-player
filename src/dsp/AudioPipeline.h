#pragma once

#include "AudioProcessor.h"

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/audio/audio.h>
#include <gst/gst.h>

#include <cstring>
#include <iostream>
#include <limits>
#include <string>

class AudioPipeline {
public:
    explicit AudioPipeline(float gain, float drive)
        : processor_(gain, drive) {}

    bool play(const std::string& uri) {
        GError* error = nullptr;

        GstElement* decoder = gst_element_factory_make("uridecodebin", "decoder");
        GstElement* converter = gst_element_factory_make("audioconvert", "converter");
        GstElement* resampler = gst_element_factory_make("audioresample", "resampler");
        GstElement* sink = gst_element_factory_make("appsink", "input-sink");
        GstElement* output = gst_parse_launch(
            "appsrc name=output-source is-live=false format=time "
            "! audioconvert ! audioresample ! autoaudiosink", &error);

        if (!decoder || !converter || !resampler || !sink || !output) {
            std::cerr << "Could not create audio pipeline.\n";
            if (error) {
                std::cerr << error->message << '\n';
                g_error_free(error);
            }
            if (decoder) gst_object_unref(decoder);
            if (converter) gst_object_unref(converter);
            if (resampler) gst_object_unref(resampler);
            if (sink) gst_object_unref(sink);
            if (output) gst_object_unref(output);
            return false;
        }
        if (error) {
            g_error_free(error);
        }

        GstCaps* requested_caps = gst_caps_from_string(
            "audio/x-raw,format=F32LE,layout=interleaved");
        g_object_set(sink, "caps", requested_caps, "sync", FALSE,
                     "max-buffers", 8u, nullptr);
        gst_caps_unref(requested_caps);
        g_object_set(decoder, "uri", uri.c_str(), nullptr);

        GstElement* input_pipeline = gst_pipeline_new("input-pipeline");
        if (!input_pipeline) {
            std::cerr << "Could not create input pipeline.\n";
            gst_object_unref(decoder);
            gst_object_unref(converter);
            gst_object_unref(resampler);
            gst_object_unref(sink);
            gst_object_unref(output);
            return false;
        }

        gst_bin_add_many(GST_BIN(input_pipeline), decoder, converter,
                         resampler, sink, nullptr);
        if (!gst_element_link_many(converter, resampler, sink, nullptr)) {
            std::cerr << "Could not link input pipeline.\n";
            gst_object_unref(input_pipeline);
            gst_object_unref(output);
            return false;
        }
        g_signal_connect(decoder, "pad-added", G_CALLBACK(onPadAdded), converter);

        GstElement* source = gst_bin_get_by_name(GST_BIN(output), "output-source");
        if (!source) {
            std::cerr << "Could not find output source.\n";
            gst_object_unref(input_pipeline);
            gst_object_unref(output);
            return false;
        }
        // Limit the amount of decoded audio waiting to be played.
        g_object_set(source, "block", TRUE, "max-bytes", static_cast<guint64>(1048576),
                     nullptr);

        if (gst_element_set_state(input_pipeline, GST_STATE_PLAYING)
            == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Could not start input pipeline.\n";
            cleanup(input_pipeline, output, source);
            return false;
        }

        bool success = true;
        bool output_started = false;
        int configured_rate = 0;
        int configured_channels = 0;

        while (true) {
            GstSample* sample = gst_app_sink_try_pull_sample(
                GST_APP_SINK(sink), 100 * GST_MSECOND);
            if (!sample) {
                if (checkBusError(input_pipeline) ||
                    (output_started && checkBusError(output))) {
                    success = false;
                    break;
                }
                if (gst_app_sink_is_eos(GST_APP_SINK(sink))) break;
                continue;
            }

            GstCaps* caps = gst_sample_get_caps(sample);
            GstAudioInfo info;
            if (!caps || !gst_audio_info_from_caps(&info, caps)) {
                std::cerr << "Failed to read audio format.\n";
                gst_sample_unref(sample);
                success = false;
                break;
            }

            const int rate = GST_AUDIO_INFO_RATE(&info);
            const int channels = GST_AUDIO_INFO_CHANNELS(&info);
            if (rate <= 2000 || channels <= 0 ||
                GST_AUDIO_INFO_FORMAT(&info) != GST_AUDIO_FORMAT_F32LE ||
                GST_AUDIO_INFO_LAYOUT(&info) != GST_AUDIO_LAYOUT_INTERLEAVED) {
                std::cerr << "Unsupported PCM audio format.\n";
                gst_sample_unref(sample);
                success = false;
                break;
            }

            if (!output_started) {
                try {
                    processor_.configureAudio(static_cast<float>(rate), channels);
                } catch (const std::exception& ex) {
                    std::cerr << "DSP configuration failed: " << ex.what() << '\n';
                    gst_sample_unref(sample);
                    success = false;
                    break;
                }
                configured_rate = rate;
                configured_channels = channels;
                g_object_set(source, "caps", caps, nullptr);
                if (gst_element_set_state(output, GST_STATE_PLAYING)
                    == GST_STATE_CHANGE_FAILURE) {
                    std::cerr << "Could not start output pipeline.\n";
                    gst_sample_unref(sample);
                    success = false;
                    break;
                }
                output_started = true;
                std::cout << "Sample rate: " << rate << " Hz\n"
                          << "Channels: " << channels << '\n';
            } else if (rate != configured_rate || channels != configured_channels) {
                // A format change requires synchronized DSP and appsrc renegotiation.
                std::cerr << "Audio format changed during playback; unsupported.\n";
                gst_sample_unref(sample);
                success = false;
                break;
            }

            GstBuffer* buffer = gst_sample_get_buffer(sample);
            GstMapInfo input_map;
            if (!buffer || !gst_buffer_map(buffer, &input_map, GST_MAP_READ)) {
                std::cerr << "Could not map input buffer.\n";
                gst_sample_unref(sample);
                success = false;
                break;
            }

            const size_t frame_bytes = sizeof(float) * static_cast<size_t>(channels);
            if (input_map.size % frame_bytes != 0 ||
                input_map.size / sizeof(float) > static_cast<size_t>(std::numeric_limits<int>::max())) {
                std::cerr << "Invalid PCM buffer size.\n";
                gst_buffer_unmap(buffer, &input_map);
                gst_sample_unref(sample);
                success = false;
                break;
            }

            GstBuffer* processed = gst_buffer_new_allocate(nullptr, input_map.size, nullptr);
            GstMapInfo output_map;
            if (!processed || !gst_buffer_map(processed, &output_map, GST_MAP_WRITE)) {
                std::cerr << "Could not allocate or map output buffer.\n";
                if (processed) gst_buffer_unref(processed);
                gst_buffer_unmap(buffer, &input_map);
                gst_sample_unref(sample);
                success = false;
                break;
            }

            std::memcpy(output_map.data, input_map.data, input_map.size);
            try {
                processor_.process(reinterpret_cast<float*>(output_map.data),
                                   static_cast<int>(input_map.size / sizeof(float)));
            } catch (const std::exception& ex) {
                std::cerr << "DSP processing failed: " << ex.what() << '\n';
                gst_buffer_unmap(processed, &output_map);
                gst_buffer_unref(processed);
                gst_buffer_unmap(buffer, &input_map);
                gst_sample_unref(sample);
                success = false;
                break;
            }
            gst_buffer_unmap(processed, &output_map);
            GST_BUFFER_PTS(processed) = GST_BUFFER_PTS(buffer);
            GST_BUFFER_DURATION(processed) = GST_BUFFER_DURATION(buffer);
            GST_BUFFER_OFFSET(processed) = GST_BUFFER_OFFSET(buffer);
            GST_BUFFER_OFFSET_END(processed) = GST_BUFFER_OFFSET_END(buffer);
            gst_buffer_unmap(buffer, &input_map);
            gst_sample_unref(sample);

            // push_buffer takes ownership of processed, including on failure.
            if (gst_app_src_push_buffer(GST_APP_SRC(source), processed) != GST_FLOW_OK) {
                std::cerr << "Could not push processed audio.\n";
                success = false;
                break;
            }
        }

        if (success && output_started) {
            if (gst_app_src_end_of_stream(GST_APP_SRC(source)) != GST_FLOW_OK) {
                std::cerr << "Could not signal end of stream.\n";
                success = false;
            } else {
                GstBus* bus = gst_element_get_bus(output);
                // Wait for actual playback completion, not a fixed 10-second limit.
                GstMessage* message = gst_bus_timed_pop_filtered(
                    bus, GST_CLOCK_TIME_NONE,
                    static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
                if (!message) {
                    std::cerr << "Output playback ended without EOS.\n";
                    success = false;
                } else {
                    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
                        printError(message);
                        success = false;
                    }
                    gst_message_unref(message);
                }
                gst_object_unref(bus);
            }
        } else if (!output_started) {
            std::cerr << "No audio samples were decoded.\n";
            success = false;
        }

        cleanup(input_pipeline, output, source);
        return success;
    }

private:
    AudioProcessor processor_;

    static void onPadAdded(GstElement*, GstPad* pad, gpointer user_data) {
        GstElement* converter = GST_ELEMENT(user_data);
        GstPad* sink_pad = gst_element_get_static_pad(converter, "sink");
        GstCaps* caps = gst_pad_get_current_caps(pad);
        if (!caps) caps = gst_pad_query_caps(pad, nullptr);
        if (caps && !gst_caps_is_empty(caps)) {
            const GstStructure* structure = gst_caps_get_structure(caps, 0);
            const char* name = gst_structure_get_name(structure);
            if (g_str_has_prefix(name, "audio/x-raw") &&
                !gst_pad_is_linked(sink_pad)) {
                if (gst_pad_link(pad, sink_pad) != GST_PAD_LINK_OK) {
                    std::cerr << "Could not link decoded audio pad.\n";
                }
            }
        }
        if (caps) gst_caps_unref(caps);
        gst_object_unref(sink_pad);
    }

    static void printError(GstMessage* message) {
        GError* error = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        std::cerr << "GStreamer error: "
                  << (error ? error->message : "Unknown") << '\n';
        if (debug) std::cerr << "Debug details: " << debug << '\n';
        g_clear_error(&error);
        g_free(debug);
    }

    static bool checkBusError(GstElement* pipeline) {
        GstBus* bus = gst_element_get_bus(pipeline);
        GstMessage* message = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR);
        const bool failed = message != nullptr;
        if (message) {
            printError(message);
            gst_message_unref(message);
        }
        gst_object_unref(bus);
        return failed;
    }

    static void cleanup(GstElement* input, GstElement* output,
                        GstElement* source) {
        gst_element_set_state(input, GST_STATE_NULL);
        gst_element_set_state(output, GST_STATE_NULL);
        gst_object_unref(source);
        gst_object_unref(input);
        gst_object_unref(output);
    }
};
