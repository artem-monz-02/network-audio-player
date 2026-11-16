#pragma once

#include "AudioProcessor.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/audio/audio.h>

#include <cmath>
#include <cstdlib>
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

        const char* headless = std::getenv("NAP_HEADLESS");

        const bool useHeadless =
            headless != nullptr &&
            std::string(headless) == "1";

        const std::string audioSink = useHeadless
            ? "fakesink sync=false"
            : "autoaudiosink";

        const std::string outputDescription =
            "appsrc name=output-source is-live=false format=time "
            "block=true max-bytes=1048576 "
            "! audioconvert "
            "! audioresample "
            "! " + audioSink;

        GstElement* output = gst_parse_launch(
            outputDescription.c_str(), &error
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

        if (error) {
            g_error_free(error);
            error = nullptr;
        }

        GstCaps* requestedCaps = gst_caps_from_string(
            "audio/x-raw,format=F32LE,layout=interleaved"
        );

        g_object_set(
            sink,
            "caps", requestedCaps,
            "sync", FALSE,
            "max-buffers", 8u,
            "drop", FALSE,
            nullptr
        );

        gst_caps_unref(requestedCaps);

        g_object_set(input, "uri", uri.c_str(), nullptr);

        GstElement* inputPipeline =
            gst_pipeline_new("input-pipeline");

        if (!inputPipeline) {
            std::cerr << "Could not create input pipeline.\n";

            gst_object_unref(input);
            gst_object_unref(converter);
            gst_object_unref(resampler);
            gst_object_unref(sink);
            gst_object_unref(output);

            return false;
        }

        gst_bin_add_many(
            GST_BIN(inputPipeline),
            input,
            converter,
            resampler,
            sink,
            nullptr
        );

        if (!gst_element_link_many(
                converter, resampler, sink, nullptr)) {
            std::cerr << "Could not link input pipeline.\n";

            gst_object_unref(inputPipeline);
            gst_object_unref(output);

            return false;
        }

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

            gst_object_unref(inputPipeline);
            gst_object_unref(output);

            return false;
        }

        if (gst_element_set_state(
                inputPipeline, GST_STATE_PLAYING
            ) == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Could not start input pipeline.\n";

            cleanup(inputPipeline, output, source);
            return false;
        }

        bool success = true;
        bool outputStarted = false;
        bool formatConfigured = false;

        int configuredRate = 0;
        int configuredChannels = 0;

        while (true) {
            GstSample* sample = gst_app_sink_try_pull_sample(
                GST_APP_SINK(sink),
                100 * GST_MSECOND
            );

            if (!sample) {
                GstBus* bus =
                    gst_element_get_bus(inputPipeline);

                GstMessage* message = gst_bus_pop_filtered(
                    bus,
                    static_cast<GstMessageType>(
                        GST_MESSAGE_ERROR |
                        GST_MESSAGE_EOS
                    )
                );

                if (message) {
                    if (GST_MESSAGE_TYPE(message) ==
                        GST_MESSAGE_ERROR) {
                        printError(message);
                        success = false;
                    }

                    gst_message_unref(message);
                }

                gst_object_unref(bus);

                if (!success ||
                    gst_app_sink_is_eos(GST_APP_SINK(sink))) {
                    break;
                }

                continue;
            }

            GstBuffer* buffer = gst_sample_get_buffer(sample);
            GstCaps* caps = gst_sample_get_caps(sample);

            GstAudioInfo audioInfo;

            if (!buffer || !caps ||
                !gst_audio_info_from_caps(&audioInfo, caps)) {
                std::cerr << "Failed to read audio format.\n";

                gst_sample_unref(sample);
                success = false;
                break;
            }

            const int sampleRate =
                GST_AUDIO_INFO_RATE(&audioInfo);

            const int channels =
                GST_AUDIO_INFO_CHANNELS(&audioInfo);

            const GstAudioFormat format =
                GST_AUDIO_INFO_FORMAT(&audioInfo);

            if (sampleRate <= 2000 ||
                channels <= 0 ||
                format != GST_AUDIO_FORMAT_F32LE ||
                GST_AUDIO_INFO_LAYOUT(&audioInfo) !=
                    GST_AUDIO_LAYOUT_INTERLEAVED) {
                std::cerr << "Unsupported audio configuration.\n";

                gst_sample_unref(sample);
                success = false;
                break;
            }

            if (!formatConfigured) {
                try {
                    processor_.configureAudio(
                        static_cast<float>(sampleRate),
                        channels
                    );
                } catch (const std::exception& exception) {
                    std::cerr << "DSP configuration failed: "
                              << exception.what() << "\n";

                    gst_sample_unref(sample);
                    success = false;
                    break;
                }

                configuredRate = sampleRate;
                configuredChannels = channels;
                formatConfigured = true;

                std::cout << "Sample rate: "
                          << sampleRate << " Hz\n";

                std::cout << "Channels: "
                          << channels << "\n";
            } else if (sampleRate != configuredRate ||
                       channels != configuredChannels) {
                std::cerr
                    << "Audio format changed during playback.\n";

                gst_sample_unref(sample);
                success = false;
                break;
            }

            if (!outputStarted) {
                g_object_set(
                    source,
                    "caps", caps,
                    nullptr
                );

                if (gst_element_set_state(
                        output, GST_STATE_PLAYING
                    ) == GST_STATE_CHANGE_FAILURE) {
                    std::cerr
                        << "Could not start output pipeline.\n";

                    gst_sample_unref(sample);
                    success = false;
                    break;
                }

                outputStarted = true;
            }

            GstMapInfo map;

            if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                std::cerr << "Could not map input buffer.\n";

                gst_sample_unref(sample);
                success = false;
                break;
            }

            if (map.size % sizeof(float) != 0 ||
                map.size % audioInfo.bpf != 0 ||
                map.size / sizeof(float) >
                    static_cast<gsize>(
                        std::numeric_limits<int>::max()
                    )) {
                std::cerr << "Invalid PCM buffer size.\n";

                gst_buffer_unmap(buffer, &map);
                gst_sample_unref(sample);
                success = false;
                break;
            }

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

            GstMapInfo outputMap;

            if (!gst_buffer_map(
                    processed, &outputMap, GST_MAP_WRITE
                )) {
                std::cerr << "Could not map output buffer.\n";

                gst_buffer_unref(processed);
                gst_buffer_unmap(buffer, &map);
                gst_sample_unref(sample);

                success = false;
                break;
            }

            std::memcpy(
                outputMap.data,
                map.data,
                map.size
            );

            float* samples =
                reinterpret_cast<float*>(outputMap.data);

            const int sampleCount =
                static_cast<int>(
                    outputMap.size / sizeof(float)
                );

            try {
                processor_.process(samples, sampleCount);
            } catch (const std::exception& exception) {
                std::cerr << "DSP processing failed: "
                          << exception.what() << "\n";

                gst_buffer_unmap(processed, &outputMap);
                gst_buffer_unref(processed);
                gst_buffer_unmap(buffer, &map);
                gst_sample_unref(sample);

                success = false;
                break;
            }

            gst_buffer_unmap(processed, &outputMap);

            GST_BUFFER_PTS(processed) =
                GST_BUFFER_PTS(buffer);

            GST_BUFFER_DTS(processed) =
                GST_BUFFER_DTS(buffer);

            GST_BUFFER_DURATION(processed) =
                GST_BUFFER_DURATION(buffer);

            GST_BUFFER_OFFSET(processed) =
                GST_BUFFER_OFFSET(buffer);

            GST_BUFFER_OFFSET_END(processed) =
                GST_BUFFER_OFFSET_END(buffer);

            gst_buffer_unmap(buffer, &map);

            GstFlowReturn result = gst_app_src_push_buffer(
                GST_APP_SRC(source), processed
            );

            gst_sample_unref(sample);

            if (result != GST_FLOW_OK) {
                std::cerr
                    << "Could not push processed audio.\n";

                success = false;
                break;
            }
        }

        if (outputStarted && success) {
            GstFlowReturn eosResult =
                gst_app_src_end_of_stream(GST_APP_SRC(source));

            if (eosResult != GST_FLOW_OK) {
                std::cerr
                    << "Could not signal end of stream.\n";

                success = false;
            }

            if (success) {
                GstBus* bus = gst_element_get_bus(output);

                GstMessage* message = gst_bus_timed_pop_filtered(
                    bus,
                    10 * GST_SECOND,
                    static_cast<GstMessageType>(
                        GST_MESSAGE_EOS |
                        GST_MESSAGE_ERROR
                    )
                );

                if (message) {
                    if (GST_MESSAGE_TYPE(message) ==
                        GST_MESSAGE_ERROR) {
                        printError(message);
                        success = false;
                    }

                    gst_message_unref(message);
                } else {
                    std::cerr
                        << "Timed out waiting for output playback.\n";

                    success = false;
                }

                gst_object_unref(bus);
            }
        } else if (!outputStarted) {
            std::cerr << "No audio samples were decoded.\n";
            success = false;
        }

        cleanup(inputPipeline, output, source);

        return success;
    }

private:
    AudioProcessor processor_;

    static void onPadAdded(
        GstElement* decoder,
        GstPad* pad,
        gpointer userData
    ) {
        (void)decoder;

        GstElement* converter =
            GST_ELEMENT(userData);

        GstPad* sinkPad =
            gst_element_get_static_pad(converter, "sink");

        if (!sinkPad) {
            return;
        }

        GstCaps* caps = gst_pad_get_current_caps(pad);

        if (!caps) {
            caps = gst_pad_query_caps(pad, nullptr);
        }

        if (caps && !gst_caps_is_empty(caps)) {
            const GstStructure* structure =
                gst_caps_get_structure(caps, 0);

            const char* name =
                gst_structure_get_name(structure);

            if (g_str_has_prefix(name, "audio/x-raw") &&
                !gst_pad_is_linked(sinkPad)) {
                GstPadLinkReturn result =
                    gst_pad_link(pad, sinkPad);

                if (result != GST_PAD_LINK_OK) {
                    std::cerr
                        << "Could not link decoded audio pad.\n";
                }
            }
        }

        if (caps) {
            gst_caps_unref(caps);
        }

        gst_object_unref(sinkPad);
    }

    static void printError(GstMessage* message) {
        GError* error = nullptr;
        gchar* debug = nullptr;

        gst_message_parse_error(
            message, &error, &debug
        );

        std::cerr << "GStreamer error: "
                  << (error ? error->message : "Unknown")
                  << "\n";

        if (debug) {
            std::cerr << "Debug details: "
                      << debug << "\n";
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
