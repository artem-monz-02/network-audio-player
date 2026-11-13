
#include <gst/gst.h>
#include <iostream>

int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    // Check if an audio file was provided
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <audio-file>\n";
        return 1;
    }

    // Convert the file path to a GStreamer URI
    gchar* absolute_path = g_canonicalize_filename(argv[1], nullptr);
    GError* error = nullptr;
    gchar* uri = gst_filename_to_uri(absolute_path, &error);
    g_free(absolute_path);

    if (uri == nullptr) {
        std::cerr << "Invalid file path.\n";
        g_clear_error(&error);
        return 1;
    }

    // Create an audio player
    GstElement* player = gst_element_factory_make("playbin", "player");

    if (player == nullptr) {
        std::cerr << "Could not create audio player.\n";
        g_free(uri);
        return 1;
    }

    // Set the audio file
    g_object_set(player, "uri", uri, nullptr);
    g_free(uri);

    // Start playback
    std::cout << "Playing: " << argv[1] << "\n";
    gst_element_set_state(player, GST_STATE_PLAYING);

    // Wait until playback finishes or an error occurs
    GstBus* bus = gst_element_get_bus(player);

    GstMessage* message = gst_bus_timed_pop_filtered(
        bus,
        GST_CLOCK_TIME_NONE,
        static_cast<GstMessageType>(
            GST_MESSAGE_EOS | GST_MESSAGE_ERROR
        )
    );

    int result = 0;

    // Check for playback errors
    if (message != nullptr) {
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
            GError* playback_error = nullptr;
            gchar* debug = nullptr;

            gst_message_parse_error(message, &playback_error, &debug);

            std::cerr << "Playback error: "
                      << (playback_error ? playback_error->message : "Unknown")
                      << "\n";

            g_clear_error(&playback_error);
            g_free(debug);
            result = 1;
        }

        gst_message_unref(message);
    }

    // Clean up
    gst_object_unref(bus);
    gst_element_set_state(player, GST_STATE_NULL);
    gst_object_unref(player);

    return result;
}

