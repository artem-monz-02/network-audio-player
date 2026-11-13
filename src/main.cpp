
#include <gst/gst.h>
#include <iostream>

int main(int argc, char* argv[])
{
    // GStreamer initialisieren
    gst_init(&argc, &argv);

    // Audio-Pipeline erstellen
    GstElement* pipeline = gst_parse_launch(
        "audiotestsrc num-buffers=200 ! "
        "audioconvert ! audioresample ! autoaudiosink",
        nullptr
    );

    // Prüfen, ob die Pipeline existiert
    if (pipeline == nullptr) {
        std::cerr << "Pipeline konnte nicht erstellt werden.\n";
        return 1;
    }

    // Audio starten
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Auf das Ende oder einen Fehler warten
    GstBus* bus = gst_element_get_bus(pipeline);

    GstMessage* message = gst_bus_timed_pop_filtered(
        bus,
        GST_CLOCK_TIME_NONE,
        static_cast<GstMessageType>(
            GST_MESSAGE_ERROR | GST_MESSAGE_EOS
        )
    );

    // Nachricht freigeben
    if (message != nullptr) {
        gst_message_unref(message);
    }

    // Aufräumen
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}

