/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace
{
struct RgbPrime
{
    float r;
    float g;
    float b;
};

constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

constexpr std::uint32_t pack_a2b10g10r10(std::uint32_t r, std::uint32_t g, std::uint32_t b,
                                         std::uint32_t a) noexcept
{
    return (r & 0x3ffU) | ((g & 0x3ffU) << 10U) | ((b & 0x3ffU) << 20U) |
           ((a & 0x3U) << 30U);
}

constexpr RgbPrime bt2020_ncl_preserve_pq(float y, float cb, float cr) noexcept
{
    return {
        y + 1.474600F * cr,
        y - 0.164553F * cb - 0.571353F * cr,
        y + 1.881400F * cb,
    };
}

constexpr float absolute(float value) noexcept
{
    return value < 0.0F ? -value : value;
}

constexpr std::size_t kPitchComponents = 1920;
constexpr std::size_t kCodedHeight = 1088;
constexpr std::size_t kYBytes = kPitchComponents * kCodedHeight * 2;
constexpr std::size_t kRawBytes = kPitchComponents * kCodedHeight * 3;
constexpr std::size_t kSlotBytes = align_up(kRawBytes, 16 * 1024);
constexpr std::uint32_t kPacked = pack_a2b10g10r10(0x155, 0x2aa, 0x3ff, 3);
constexpr RgbPrime kNeutral = bt2020_ncl_preserve_pq(0.5F, 0.0F, 0.0F);

static_assert(kRawBytes == 0x5fa000);
static_assert(kSlotBytes == 0x5fc000);
static_assert((kPacked & 0x3ffU) == 0x155U);
static_assert(((kPacked >> 10U) & 0x3ffU) == 0x2aaU);
static_assert(((kPacked >> 20U) & 0x3ffU) == 0x3ffU);
static_assert((kPacked >> 30U) == 3U);
static_assert(absolute(kNeutral.r - 0.5F) < 0.00001F);
static_assert(absolute(kNeutral.g - 0.5F) < 0.00001F);
static_assert(absolute(kNeutral.b - 0.5F) < 0.00001F);
} // namespace

int main()
{
    std::cout << std::hex << "Main10 Y bytes:  0x" << kYBytes << '\n'
              << "Main10 UV offset: 0x" << kYBytes << '\n'
              << "Raw/aligned size: 0x" << kRawBytes << " / 0x" << kSlotBytes << '\n'
              << "A2B10G10R10 word: 0x" << std::setw(8) << std::setfill('0') << kPacked
              << std::dec << std::setfill(' ') << '\n'
              << std::fixed << std::setprecision(3) << "Neutral PQ-coded RGB remains "
              << kNeutral.r << '/' << kNeutral.g << '/' << kNeutral.b << '\n';
}
