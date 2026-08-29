/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
constexpr std::size_t kDirectAlignment = 16U * 1024U;
constexpr std::size_t kAuSlotBytes = kDirectAlignment;
constexpr std::size_t kPitch = 16;
constexpr std::size_t kCodedHeight = 8;
constexpr std::size_t kNv12Bytes = kPitch * kCodedHeight * 3U / 2U;
constexpr std::size_t kFrameSlotBytes = kDirectAlignment;
constexpr std::size_t kSlotCount = 3;

class AlignedBytes
{
public:
    explicit AlignedBytes(std::size_t size) : storage_(size + kDirectAlignment - 1U)
    {
        const auto address = reinterpret_cast<std::uintptr_t>(storage_.data());
        const auto padding = (kDirectAlignment - address % kDirectAlignment) % kDirectAlignment;
        bytes_ = {storage_.data() + padding, size};
    }

    std::span<std::byte> bytes() noexcept { return bytes_; }

private:
    std::vector<std::byte> storage_;
    std::span<std::byte> bytes_;
};

enum class Owner
{
    free,
    receiver,
    decoder,
    presenter,
};

struct Slot
{
    std::span<std::byte> bytes;
    Owner owner{Owner::free};
};

template <std::size_t Count>
std::array<Slot, Count> split_into_slots(std::span<std::byte> memory, std::size_t slot_bytes)
{
    if (slot_bytes % kDirectAlignment != 0 || memory.size() < Count * slot_bytes)
    {
        throw std::invalid_argument("invalid direct-memory pool geometry");
    }

    std::array<Slot, Count> slots{};
    for (std::size_t index = 0; index < Count; ++index)
    {
        slots[index].bytes = memory.subspan(index * slot_bytes, slot_bytes);
        assert(reinterpret_cast<std::uintptr_t>(slots[index].bytes.data()) % kDirectAlignment == 0);
    }
    return slots;
}

Slot &acquire(std::span<Slot> slots, Owner owner)
{
    const auto available = std::find_if(slots.begin(), slots.end(), [](const Slot &slot) {
        return slot.owner == Owner::free;
    });
    if (available == slots.end())
    {
        throw std::runtime_error("bounded pool exhausted");
    }
    available->owner = owner;
    return *available;
}

std::size_t gather_in_order(std::span<const std::span<const std::byte>> fragments, Slot &slot)
{
    if (slot.owner != Owner::receiver)
    {
        throw std::logic_error("input slot is not owned by receiver");
    }

    std::size_t used = 0;
    for (const auto fragment : fragments)
    {
        if (fragment.size() > slot.bytes.size() - used)
        {
            throw std::length_error("encoded access unit exceeds slot");
        }
        std::copy(fragment.begin(), fragment.end(), slot.bytes.begin() + used);
        used += fragment.size();
    }
    return used;
}

struct DecoderInput
{
    const std::byte *data;
    std::size_t size;
    std::uint64_t presentation_timestamp;
};

struct DecoderOutput
{
    std::byte *buffer;
    std::size_t bytes;
    std::size_t pitch;
    std::size_t coded_height;
    bool valid;
    bool error;
};

DecoderOutput decode(DecoderInput input, Slot &au_slot, Slot &frame_slot)
{
    if (au_slot.owner != Owner::decoder || frame_slot.owner != Owner::decoder ||
        input.data != au_slot.bytes.data() || input.size > au_slot.bytes.size())
    {
        throw std::logic_error("decoder received an invalid slot");
    }

    // The real media engine writes this caller-supplied frame slot. The host
    // example models only pointer ownership, not compressed-video decoding.
    au_slot.owner = Owner::free;
    return {frame_slot.bytes.data(), kNv12Bytes, kPitch, kCodedHeight, true, false};
}

bool owns(std::span<const Slot> slots, const std::byte *pointer) noexcept
{
    return std::any_of(slots.begin(), slots.end(), [pointer](const Slot &slot) {
        return slot.bytes.data() == pointer;
    });
}

struct Nv12Planes
{
    std::span<const std::byte> y;
    std::span<const std::byte> uv;
};

Nv12Planes describe_nv12(const DecoderOutput &output, const Slot &slot)
{
    if (!output.valid || output.error || output.buffer != slot.bytes.data() ||
        output.coded_height == 0 || output.pitch > slot.bytes.size() / output.coded_height)
    {
        throw std::runtime_error("invalid decoder output");
    }

    const std::size_t y_bytes = output.pitch * output.coded_height;
    const std::size_t uv_bytes = y_bytes / 2U;
    if (output.bytes != y_bytes + uv_bytes || output.bytes > slot.bytes.size())
    {
        throw std::runtime_error("decoded surface exceeds frame slot");
    }
    return {{output.buffer, y_bytes}, {output.buffer + y_bytes, uv_bytes}};
}

struct FlipFence
{
    const std::byte *source;
};

FlipFence present_same_pointer(const DecoderOutput &output, const Nv12Planes &planes,
                               Slot &frame_slot)
{
    if (frame_slot.owner != Owner::decoder || planes.y.data() != output.buffer)
    {
        throw std::logic_error("presenter did not receive decoder output directly");
    }
    frame_slot.owner = Owner::presenter;
    return {output.buffer};
}

void complete_flip(FlipFence fence, Slot &frame_slot)
{
    if (frame_slot.owner != Owner::presenter || fence.source != frame_slot.bytes.data())
    {
        throw std::logic_error("completed flip does not own this frame slot");
    }
    frame_slot.owner = Owner::free;
}
} // namespace

int main()
{
    AlignedBytes au_memory{kAuSlotBytes * kSlotCount};
    AlignedBytes frame_memory{kFrameSlotBytes * kSlotCount};
    auto au_pool = split_into_slots<kSlotCount>(au_memory.bytes(), kAuSlotBytes);
    auto frame_pool = split_into_slots<kSlotCount>(frame_memory.bytes(), kFrameSlotBytes);

    constexpr std::array first_fragment{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    constexpr std::array second_fragment{std::byte{0x04}, std::byte{0x05}};
    const std::array<const std::span<const std::byte>, 2> fragments{
        std::span<const std::byte>{first_fragment}, std::span<const std::byte>{second_fragment}};

    Slot &au_slot = acquire(au_pool, Owner::receiver);
    Slot &frame_slot = acquire(frame_pool, Owner::decoder);
    const std::size_t compressed_bytes = gather_in_order(fragments, au_slot);
    au_slot.owner = Owner::decoder;

    const DecoderInput input{au_slot.bytes.data(), compressed_bytes, 1};
    const DecoderOutput output = decode(input, au_slot, frame_slot);
    if (!owns(frame_pool, output.buffer))
    {
        throw std::runtime_error("decoder returned memory outside caller frame pool");
    }

    const Nv12Planes planes = describe_nv12(output, frame_slot);
    const FlipFence fence = present_same_pointer(output, planes, frame_slot);
    complete_flip(fence, frame_slot);

    std::cout << "compressed bytes gathered: " << compressed_bytes << '\n'
              << "decoder output is caller frame slot: " << std::boolalpha
              << (output.buffer == frame_slot.bytes.data()) << '\n'
              << "presenter Y plane is decoder output: " << (planes.y.data() == output.buffer)
              << '\n'
              << "UV offset: " << (planes.uv.data() - planes.y.data()) << " bytes\n"
              << "frame released after completed flip: " << (frame_slot.owner == Owner::free)
              << '\n';
}
