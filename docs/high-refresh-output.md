# High-refresh and native 4K output

## Result

The PS5 application path can present a true 3840x2160 source through a native
3840x2160 VideoOut target at the platform's 119.88 Hz mode. Two isolated
controls presented all 600 requested frames:

| Firmware | Source | VideoOut target | Requested cadence | Measured cadence | Result |
|---|---:|---:|---:|---:|---|
| 6.02 | 3840x2160 | 3840x2160 | 120 FPS | 119.87 FPS | 600/600 frames; clean teardown |
| 12.70 | 3840x2160 | 3840x2160 | 120 FPS | 119.88 FPS | 600/600 frames; clean teardown |

Earlier isolated 1080p controls measured 89.99 FPS for a 90 FPS target and
119.85 FPS for a 120 FPS target. A 120 FPS loading-animation control measured
109.33 FPS because it regenerated and flushed a complete CPU-written 1080p
surface for every frame; the fixed-source presenter still reached 119.85 FPS.
That comparison rules out a general 60 FPS GPU or VideoOut ceiling.

The production integration subsequently accepted live H.264 Sunshine sessions
at 1080p/120, 1440p/120, and 2160p/120. Separate logs matched a 1440p/90 client
request to a 90 Hz host capture and a 2160p/120 request to a 120 Hz host capture.
This is end-to-end product acceptance, not a controlled decoder-latency
distribution.

## What is proven

- The ordinary application VideoOut configuration route can select HFR when
  the title advertises the corresponding high-refresh and high-resolution
  capability.
- No VR renderer or restricted system VideoOut interface is required.
- AGC can render a true 3840x2160 source into native 3840x2160 scanout fast
  enough for the 119.88 Hz display interval in the tested fixed-source oracle.
- The same presentation contract works on firmware 6.02 and 12.70.
- VideoDec2, AGC, VideoOut, Moonlight protocol handling, audio, and input can
  coexist in a live 2160p/120 H.264 session.

The result does **not** prove that every codec, bitrate, encoder preset, frame
structure, or game can produce and decode 120 distinct 4K frames per second.
At 119.88 Hz, one display interval is about 8.34 ms. The practical live HEVC
4K60 stream averaged 5.464 ms synchronous decode and therefore had average
decoder headroom inside that interval, while the controlled roughly 72 Mb/s
HEVC stream averaged 20.719 ms at depth one. Content and bitstream structure
matter more than the resolution label alone.

## Resolution-aware output geometry

The accepted product policy is:

| Stream source | VideoOut target | Mapping |
|---:|---:|---|
| 1920x1080 | 1920x1080 | 1:1 |
| 2560x1440 | 3840x2160 | AGC filtered scale |
| 3840x2160 | 3840x2160 | 1:1 visible picture |

Native 2560x1440 scanout has not been established through this application
interface. Keeping the physical target at 3840x2160 avoids first reducing a
1440p source to 1080p and lets the GPU perform the one required scale. For a
4K stream, retain coded and visible geometry separately: the validated live
layout can be 3840x2176 coded with a 3840x2160 visible crop.

The first HFR experiments preserved the full negotiated decoder resolution but
used a legacy 1920x1080 HFR output target. Those sessions proved decode and HFR
timing, not native high-resolution scanout. Adding the high-resolution title
capability while retaining the ordinary HFR VideoOut route allowed 3840x2160
buffers to register and the display to enter native 3840x2160 at 119.88 Hz.

## Presenter scheduling

HFR requires more than selecting a refresh mode:

1. Allocate and register framebuffer geometry for the selected physical target.
2. Keep decoded surfaces owned until their AGC and flip work completes.
3. Rotate display buffers so CPU/network/decode work does not serialize behind
   every individual vblank.
4. Submit only the flip integrated into the AGC command buffer; a second flip
   halves effective cadence.
5. Drain outstanding GPU and VideoOut work before unregistering buffers or
   releasing direct memory.
6. Restore the normal launcher output at the stream boundary.

The isolated HFR oracle used three surfaces to separate producer progress from
display completion. The later product implementation also enforces bounded
ownership before reuse; buffer count is an implementation choice, not a reason
to permit unbounded presentation work.

For a 90 FPS stream, the tested implementation uses the available HFR output
and unpegs fixed-rate presentation so application pacing can settle near 90 FPS.
For 120 FPS, it retains fixed 119.88 Hz output. Measure completed flips rather
than equating requested frame rate, decoder submissions, or host capture rate
with visible cadence.

## HDMI transition and startup behavior

Declaring HFR capability can make the console switch the HDMI link before
application code draws its first frame. On one console/HDMI-port combination,
the television briefly displayed black while locking to 119.88 Hz; another
console on a different, more capable HDMI port showed the transition normally.
The launcher itself then remained at the HFR scanout cadence.

This interval is display-link resynchronization, not evidence that startup code
is blocked. An application-owned startup image can bridge the interval after
the TV regains sync, but it cannot draw during the period in which the display
has not yet locked to the new mode. Avoid diagnosing this case only from a
remote-capture client, which may not reproduce the physical HDMI transition.

Early capability mismatches produced black output, a top-left quarter image,
or an unrelated VR-requirement error. Those symptoms did not indicate a real VR
dependency. They occurred when source, framebuffer, title capability, and
physical output geometry did not describe one consistent HFR configuration.

## Live-stream configuration

Choose resolution, frame rate, codec, HDR state, and bitrate before launching
the Sunshine application. If those settings change after returning to the
launcher, stop the active Sunshine application and start a new session. A
resumed host capture can retain state from the previous negotiation even when
the client has rebuilt its local decoder and presenter.

Treat bitrate as a per-configuration tuning value:

- begin with a moderate value and raise it while watching dropped frames,
  queue depth, decode-ready time, and input latency;
- prefer wired Ethernet for 1440p/2160p and 90/120 FPS testing;
- do not assume a 1 Gb/s link makes a 500 Mb/s stream useful or smooth;
- remeasure after changing codec, encoder preset, slices/WPP, content, or HDR;
  and
- drop stale presentation work rather than letting latency accumulate when the
  producer temporarily outruns decode or display.

Very high bitrate can increase encoder latency, access-unit size, receive
burstiness, decoder work, and queue residency at once. A lower bitrate that
keeps every stage inside the frame budget is usually the better low-latency
setting.

## Measurement checklist for 4K/120

Record all of these boundaries before calling a mode sustained:

```text
host capture rate and exact client request
encoded bytes/bitrate, codec profile, slices/WPP, B frames
network first-byte and complete-AU time
receive/jitter queue depth and stale-frame drops
decode submission and submission-to-ready percentiles
AGC submission and completed-flip marker
physical output dimensions and refresh mode
presented FPS, audio backlog, A/V sync, and input latency
resource cleanup after stop/reconnect
```

The most important distinction is between **presentation capacity** and
**end-to-end stream capacity**. The 600-frame oracles prove native 4K/119.88 Hz
presentation. The live ProsperoLight sessions prove that real streaming can use
that path. A controlled 4K/120 decoder-to-completed-flip soak across codecs and
bitrates remains the next measurement needed for universal performance claims.

## Evidence boundary

The HFR work used isolated authorized test titles and the independently
developed [ProsperoLight](https://github.com/blackbearreloaded/ProsperoLight)
client. This public document intentionally records semantic behavior and
measurements, not private selector values, development title identifiers,
signed artifacts, console addresses, or implementation extracts.
