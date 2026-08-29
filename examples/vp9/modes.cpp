/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../common/video_mode.hpp"

#include <array>

namespace
{
using ps5_video_example::VideoMode;

constexpr std::array<VideoMode, 5> kModes{{
    {"profile0", "VP9 Profile 0", "4.1", 1920, 1080, 1920, 1080, 1920, 1080, 0, 2048,
     2048, "8-bit two-plane"},
    {"profile0", "VP9 Profile 0", "5.0", 2560, 1440, 2560, 1440, 2560, 1440, 0, 2560,
     2560, "8-bit two-plane"},
    {"profile0", "VP9 Profile 0", "5.1", 3840, 2160, 3840, 2160, 3840, 2160, 0, 3840,
     3840, "8-bit two-plane"},
    {"profile2", "VP9 Profile 2", "4.1", 1920, 1080, 1920, 1080, 1920, 1080, 0, 1920,
     3840, "low-aligned 10-bit 4:2:0"},
    {"profile2", "VP9 Profile 2", "5.1", 3840, 2160, 3840, 2160, 3840, 2160, 0, 3840,
     7680, "low-aligned 10-bit 4:2:0"},
}};

static_assert(ps5_video_example::find_mode(kModes, "profile0", 1080)->pitch_bytes == 2048);
static_assert(ps5_video_example::find_mode(kModes, "profile2", 2160)->pitch_bytes == 7680);
static_assert(ps5_video_example::find_mode(kModes, "profile2", 1440) == nullptr);
} // namespace

int main(int argc, char **argv)
{
    return ps5_video_example::run(kModes, "profile0|profile2", argc, argv);
}
