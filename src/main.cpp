
#include <gst/gst.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <file-or-url>\n";
        return 1;
    }

    std::string input = argv[1];
    gchar* uri = nullptr;

    // Check whether the input is a URL
    if (input.find("http://") == 0 ||
        input.find("https://") == 0) {
        uri = g_strdup(input.c_str());
    }
    else {
        // Convert a local file path into a URI
        gchar* path = g_canonicalize_filename(argv[1], nullptr);
        uri = gst_filename_to_uri(path, nullptr);
        g_free(path);
    }

    if (uri == nullptr) {
        std::cerr << "Invalid file path or URL.\n";
        return 1;
    }

    // Create the player
    GstElement* player = gst_element_factory_make("playbin", "player");

    if (player == nullptr) {
        std::cerr << "Could not create player.\n";
        g_free(uri);
        return 1;
    }

    // Set the audio source
    g_object_set(player, "uri", uri, nullptr);

// Use souphttpsrc explicitly for HTTP streaming
if (input.find("http://") == 0 ||
    input.find("https://") == 0) {

    GstElement* source = gst_element_factory_make(
        "souphttpsrc", "http-source"
    );

    if (source == nullptr) {
        std::cerr << "Could not create HTTP source.\n";
        g_free(uri);
        gst_object_unref(player);
        return 1;
    }

    g_object_set(player, "source", source, nullptr);
    gst_object_unref(source);
}
    g_free(uri);

    // Start playback
    std::cout << "Playing: " << input << "\n";

    GstStateChangeReturn state = gst_element_set_state(
        player, GST_STATE_PLAYING
    );

    if (state == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Could not start playback.\n";
        gst_element_set_state(player, GST_STATE_NULL);
        gst_object_unref(player);
        return 1;
    }

    // Wait for playback to finish or fail
    GstBus* bus = gst_element_get_bus(player);

    GstMessage* message = gst_bus_timed_pop_filtered(
        bus,
        GST_CLOCK_TIME_NONE,
        static_cast<GstMessageType>(
            GST_MESSAGE_EOS | GST_MESSAGE_ERROR
        )
    );

    int result = 0;

    if (message == nullptr) {
        std::cerr << "No playback message received.\n";
        result = 1;
    }
    else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        GError* error = nullptr;
        gchar* debug = nullptr;

        gst_message_parse_error(message, &error, &debug);

        std::cerr << "Playback error: "
                  << (error ? error->message : "Unknown")
                  << "\n";

        g_clear_error(&error);
        g_free(debug);
        result = 1;
    }
    else {
        std::cout << "Playback finished.\n";
    }

    if (message != nullptr) {
        gst_message_unref(message);
    }

    // Clean up
    gst_object_unref(bus);
    gst_element_set_state(player, GST_STATE_NULL);
    gst_object_unref(player);

    return result;
}

