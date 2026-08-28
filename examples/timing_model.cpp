/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <chrono>
#include <iostream>
#include <string_view>

namespace
{
using Microseconds = std::chrono::microseconds;

struct FrameTimestamps
{
    Microseconds callback;
    Microseconds submit_begin;
    Microseconds submit_end;
    Microseconds output_ready;
    Microseconds completed_flip;
};

constexpr Microseconds gather_time(const FrameTimestamps &timestamps) noexcept
{
    return timestamps.submit_begin - timestamps.callback;
}

constexpr Microseconds submission_time(const FrameTimestamps &timestamps) noexcept
{
    return timestamps.submit_end - timestamps.submit_begin;
}

constexpr Microseconds ready_time(const FrameTimestamps &timestamps) noexcept
{
    return timestamps.output_ready - timestamps.submit_begin;
}

constexpr Microseconds callback_to_flip(const FrameTimestamps &timestamps) noexcept
{
    return timestamps.completed_flip - timestamps.callback;
}

void print_metrics(std::string_view name, const FrameTimestamps &timestamps)
{
    std::cout << name << '\n'
              << "  gather:             " << gather_time(timestamps).count() << " us\n"
              << "  submission call:    " << submission_time(timestamps).count() << " us\n"
              << "  submission-to-ready:" << ready_time(timestamps).count() << " us\n"
              << "  callback-to-flip:   " << callback_to_flip(timestamps).count() << " us\n";
}

constexpr FrameTimestamps kDepthOne{Microseconds{0}, Microseconds{5}, Microseconds{5469},
                                    Microseconds{5477}, Microseconds{16698}};
constexpr FrameTimestamps kDepthThree{Microseconds{0}, Microseconds{5}, Microseconds{338},
                                      Microseconds{33715}, Microseconds{50059}};

static_assert(submission_time(kDepthOne) == Microseconds{5464});
static_assert(ready_time(kDepthOne) == Microseconds{5472});
static_assert(submission_time(kDepthThree) == Microseconds{333});
static_assert(ready_time(kDepthThree) == Microseconds{33710});
} // namespace

int main()
{
    print_metrics("Practical 4K60 depth one", kDepthOne);
    print_metrics("Practical 4K60 depth three", kDepthThree);
    std::cout << "A short submission call is not necessarily a short decode latency.\n";
}
