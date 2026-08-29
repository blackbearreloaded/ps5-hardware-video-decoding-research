/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using Bytes = std::span<const std::byte>;

std::vector<Bytes> split_superframe(Bytes packet)
{
    if (packet.empty())
    {
        throw std::invalid_argument("empty VP9 packet");
    }

    const auto marker = std::to_integer<std::uint8_t>(packet.back());
    if ((marker & 0xe0U) != 0xc0U)
    {
        return {packet};
    }

    const std::size_t frame_count = (marker & 0x07U) + 1U;
    const std::size_t bytes_per_size = ((marker >> 3U) & 0x03U) + 1U;
    const std::size_t index_size = 2U + frame_count * bytes_per_size;
    if (index_size > packet.size())
    {
        throw std::invalid_argument("truncated VP9 superframe index");
    }

    const std::size_t index_offset = packet.size() - index_size;
    if (std::to_integer<std::uint8_t>(packet[index_offset]) != marker)
    {
        throw std::invalid_argument("mismatched VP9 superframe markers");
    }

    std::vector<Bytes> frames;
    frames.reserve(frame_count);
    std::size_t frame_offset = 0;
    std::size_t size_offset = index_offset + 1U;
    for (std::size_t frame = 0; frame < frame_count; ++frame)
    {
        std::size_t frame_size = 0;
        for (std::size_t byte = 0; byte < bytes_per_size; ++byte)
        {
            frame_size |= static_cast<std::size_t>(
                              std::to_integer<std::uint8_t>(packet[size_offset++]))
                          << (byte * 8U);
        }
        if (frame_size == 0 || frame_size > index_offset - frame_offset)
        {
            throw std::invalid_argument("invalid VP9 superframe size");
        }
        frames.push_back(packet.subspan(frame_offset, frame_size));
        frame_offset += frame_size;
    }
    if (frame_offset != index_offset)
    {
        throw std::invalid_argument("VP9 superframe sizes do not cover payload");
    }
    return frames;
}

struct FrameFlags
{
    bool show_frame;
    bool show_existing_frame;
};

constexpr bool submit_to_decoder(FrameFlags) noexcept
{
    return true;
}

constexpr bool present_decoder_output(FrameFlags flags) noexcept
{
    return flags.show_frame || flags.show_existing_frame;
}

static_assert(submit_to_decoder({false, false}));
static_assert(!present_decoder_output({false, false}));
static_assert(present_decoder_output({false, true}));
} // namespace

int main()
{
    constexpr std::array<std::byte, 7> packet{
        std::byte{0x11}, std::byte{0x22}, std::byte{0x33},
        std::byte{0xc1}, std::byte{0x02}, std::byte{0x01}, std::byte{0xc1}};
    const auto frames = split_superframe(packet);
    if (frames.size() != 2 || frames[0].size() != 2 || frames[1].size() != 1)
    {
        return 1;
    }

    std::cout << "coded frames: " << frames.size() << '\n'
              << "hidden frame: submit=yes, present="
              << (present_decoder_output({false, false}) ? "yes" : "no") << '\n'
              << "show-existing frame: submit=yes, present="
              << (present_decoder_output({false, true}) ? "yes" : "no") << '\n';
}
