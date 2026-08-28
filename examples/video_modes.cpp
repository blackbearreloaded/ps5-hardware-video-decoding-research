/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <array>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace
{
struct VideoMode
{
    std::string_view codec_name;
    std::uint32_t visible_height;
    std::string_view profile;
    std::string_view maximum_level;
    std::uint32_t maximum_width;
    std::uint32_t maximum_height;
    std::uint32_t coded_width;
    std::uint32_t coded_height;
    std::uint32_t alternate_coded_height;
    std::uint32_t pitch_components;
    std::uint32_t pitch_bytes;
    std::uint32_t visible_width;
    std::string_view surface;
};

constexpr std::array<VideoMode, 7> kModes{{
    {"h264", 1080, "High", "5.1", 1920, 1088, 1920, 1088, 0, 2048, 2048, 1920,
     "NV12"},
    {"h264", 1440, "High", "5.1", 2560, 1440, 2560, 1440, 0, 2560, 2560, 2560,
     "NV12"},
    {"h264", 2160, "High", "5.2", 3840, 2176, 3840, 2160, 0, 3840, 3840, 3840,
     "NV12"},
    {"hevc", 1080, "Main", "4.1", 1920, 1088, 1920, 1088, 0, 2048, 2048, 1920,
     "NV12"},
    {"hevc", 1440, "Main", "5.0", 2560, 1440, 2560, 1440, 0, 2560, 2560, 2560,
     "NV12"},
    {"hevc", 2160, "Main", "5.1", 3840, 2176, 3840, 2160, 2176, 3840, 3840, 3840,
     "NV12"},
    {"main10", 1080, "Main10", "4.1", 1920, 1088, 1920, 1088, 0, 1920, 3840, 1920,
     "low-aligned 10-bit 4:2:0"},
}};

constexpr const VideoMode *find_mode(std::string_view codec, std::uint32_t visible_height) noexcept
{
    for (const auto &mode : kModes)
    {
        if (mode.codec_name == codec && mode.visible_height == visible_height)
        {
            return &mode;
        }
    }
    return nullptr;
}

constexpr bool accepts_coded_height(const VideoMode &mode, std::uint32_t height) noexcept
{
    return height == mode.coded_height ||
           (mode.alternate_coded_height != 0 && height == mode.alternate_coded_height);
}

bool parse_height(std::string_view text, std::uint32_t &height) noexcept
{
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), height);
    return error == std::errc{} && end == text.data() + text.size();
}

static_assert(find_mode("h264", 1080) != nullptr);
static_assert(find_mode("hevc", 2160)->alternate_coded_height == 2176);
static_assert(find_mode("main10", 1440) == nullptr);
} // namespace

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: " << argv[0] << " h264|hevc|main10 1080|1440|2160\n";
        return 2;
    }

    std::uint32_t height = 0;
    if (!parse_height(argv[2], height))
    {
        std::cerr << "height must be an integer\n";
        return 2;
    }

    const VideoMode *mode = find_mode(argv[1], height);
    if (mode == nullptr)
    {
        std::cerr << "mode is not in the proven table\n";
        return 2;
    }
    if (!accepts_coded_height(*mode, mode->coded_height))
    {
        return 1;
    }

    std::cout << mode->codec_name << ' ' << mode->visible_width << 'x' << mode->visible_height
              << " visible\n"
              << "  profile: " << mode->profile << "; max level: " << mode->maximum_level << '\n'
              << "  config maximum: " << mode->maximum_width << 'x' << mode->maximum_height
              << '\n'
              << "  coded output: " << mode->coded_width << 'x' << mode->coded_height;
    if (mode->alternate_coded_height != 0)
    {
        std::cout << " or " << mode->coded_width << 'x' << mode->alternate_coded_height;
    }
    std::cout << "\n  pitch: " << mode->pitch_components << " components, " << mode->pitch_bytes
              << " bytes; surface: " << mode->surface << '\n';
}
