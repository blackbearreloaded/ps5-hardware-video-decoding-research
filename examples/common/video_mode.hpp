/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <charconv>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

namespace ps5_video_example
{
struct VideoMode
{
    std::string_view selector;
    std::string_view profile;
    std::string_view maximum_level;
    std::uint32_t visible_width;
    std::uint32_t visible_height;
    std::uint32_t maximum_width;
    std::uint32_t maximum_height;
    std::uint32_t coded_width;
    std::uint32_t coded_height;
    std::uint32_t alternate_coded_height;
    std::uint32_t pitch_components;
    std::uint32_t pitch_bytes;
    std::string_view surface;
};

constexpr const VideoMode *find_mode(std::span<const VideoMode> modes,
                                     std::string_view selector,
                                     std::uint32_t visible_height) noexcept
{
    for (const auto &mode : modes)
    {
        if (mode.selector == selector && mode.visible_height == visible_height)
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

inline bool parse_height(std::string_view text, std::uint32_t &height) noexcept
{
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), height);
    return error == std::errc{} && end == text.data() + text.size();
}

inline int run(std::span<const VideoMode> modes, std::string_view usage, int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: " << argv[0] << ' ' << usage << " 1080|1440|2160\n";
        return 2;
    }

    std::uint32_t height = 0;
    if (!parse_height(argv[2], height))
    {
        std::cerr << "height must be an integer\n";
        return 2;
    }

    const VideoMode *mode = find_mode(modes, argv[1], height);
    if (mode == nullptr)
    {
        std::cerr << "mode is not in the proven table\n";
        return 2;
    }
    if (!accepts_coded_height(*mode, mode->coded_height))
    {
        return 1;
    }

    std::cout << mode->profile << ' ' << mode->visible_width << 'x' << mode->visible_height
              << " visible\n"
              << "  max level: " << mode->maximum_level << '\n'
              << "  config maximum: " << mode->maximum_width << 'x' << mode->maximum_height
              << '\n'
              << "  coded output: " << mode->coded_width << 'x' << mode->coded_height;
    if (mode->alternate_coded_height != 0)
    {
        std::cout << " or " << mode->coded_width << 'x' << mode->alternate_coded_height;
    }
    std::cout << "\n  pitch: " << mode->pitch_components << " components, "
              << mode->pitch_bytes << " bytes; surface: " << mode->surface << '\n';
    return 0;
}
} // namespace ps5_video_example
