#include <gst/gst.h>

#include "dsp/AudioPipeline.h"

#include <iostream>
#include <string>
#include <cmath>

int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    if (argc < 2) {
        std::cout << "Usage: " << argv[0]
                  << " <file-or-url> [gain]\n";
        return 1;
    }

    std::string input = argv[1];
    float gain = 1.0f;
    float drive = 0.0f;

    try {
        if (argc >= 3) {
            gain = std::stof(argv[2]);
        }

        if (argc >= 4) {
            drive = std::stof(argv[3]);
        }
    }
    catch (...) {
        std::cerr << "Invalid gain or drive value.\n";
        return 1;
    }

    if (!std::isfinite(gain) || !std::isfinite(drive) ||
        gain < 0.0f || drive < 0.0f) {
        std::cerr << "Gain and drive must be finite, non-negative values.\n";
        return 1;
    }

    std::string uri;

    if (input.find("http://") == 0 ||
        input.find("https://") == 0) {
        uri = input;
    }
    else {
        gchar* path = g_canonicalize_filename(argv[1], nullptr);
        gchar* file_uri = gst_filename_to_uri(path, nullptr);

        g_free(path);

        if (!file_uri) {
            std::cerr << "Invalid file path.\n";
            return 1;
        }

        uri = file_uri;
        g_free(file_uri);
    }

    std::cout << "Playing: " << input << "\n";
    std::cout << "Gain: " << gain << "\n";

    AudioPipeline pipeline(gain, drive);

    if (!pipeline.play(uri)) {
        return 1;
    }

    std::cout << "Playback finished.\n";
    return 0;
}
