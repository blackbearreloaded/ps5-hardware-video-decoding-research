/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../common/video_mode.hpp"

#include <array>

namespace
{
using ps5_video_example::VideoMode;

constexpr std::array<VideoMode, 3> kModes{{
    {"high", "H.264 High", "5.1", 1920, 1080, 1920, 1088, 1920, 1088, 0, 2048, 2048,
     "NV12"},
    {"high", "H.264 High", "5.1", 2560, 1440, 2560, 1440, 2560, 1440, 0, 2560, 2560,
     "NV12"},
    {"high", "H.264 High", "5.2", 3840, 2160, 3840, 2176, 3840, 2160, 0, 3840, 3840,
     "NV12"},
}};

static_assert(ps5_video_example::find_mode(kModes, "high", 1080)->coded_height == 1088);
static_assert(ps5_video_example::find_mode(kModes, "high", 2160)->pitch_bytes == 3840);
} // namespace

int main(int argc, char **argv)
{
    return ps5_video_example::run(kModes, "high", argc, argv);
}
