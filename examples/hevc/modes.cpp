/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../common/video_mode.hpp"

#include <array>

namespace
{
using ps5_video_example::VideoMode;

constexpr std::array<VideoMode, 4> kModes{{
    {"main", "HEVC Main", "4.1", 1920, 1080, 1920, 1088, 1920, 1088, 0, 2048, 2048,
     "NV12"},
    {"main", "HEVC Main", "5.0", 2560, 1440, 2560, 1440, 2560, 1440, 0, 2560, 2560,
     "NV12"},
    {"main", "HEVC Main", "5.1", 3840, 2160, 3840, 2176, 3840, 2160, 2176, 3840, 3840,
     "NV12"},
    {"main10", "HEVC Main10", "4.1", 1920, 1080, 1920, 1088, 1920, 1088, 0, 1920, 3840,
     "low-aligned 10-bit 4:2:0"},
}};

static_assert(ps5_video_example::find_mode(kModes, "main", 2160)->alternate_coded_height == 2176);
static_assert(ps5_video_example::find_mode(kModes, "main10", 1440) == nullptr);
} // namespace

int main(int argc, char **argv)
{
    return ps5_video_example::run(kModes, "main|main10", argc, argv);
}
