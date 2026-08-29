# Evidence, scope, and reproducibility

## Test boundary

The console experiments ran on one PlayStation 5 with firmware 6.02 between
2026-08-24 and 2026-08-28. They used isolated, authorized test applications and a native
real-time streaming prototype. Every successful milestone fixed the tested
build and bitstream, validated exact caller-pool pointer identity, recorded
structured results, and shut down cleanly.

This public guide intentionally omits console addresses, credentials, signed
artifacts, firmware/application binaries, implementation extracts, and local
laboratory records. It retains the independently written engineering result and
measurement boundary.

## Evidence groups

| Evidence group | Contribution retained here |
|---|---|
| H.264 and HEVC console runs | Known-good modes, caller-owned surfaces, resolution comparisons, pipeline depth, WPP, and practical 4K60 timing |
| HDR and Main10 console runs | Required application capability, 10-bit VideoOut, channel packing, BT.2020/PQ conversion, and Main10 surface layout |
| Bounded AV1 interface review | Firmware-6.02 API/backend conclusion with AVC, HEVC, and VP9 positive controls |
| Controlled VP9 console run | Profile 0 decoder lifecycle, single-frame output metadata, and caller-owned surface identity |
| Decoder/presenter prototype | Zero-copy pointer identity and integer luma/chroma sampling behavior |
| Real-time streaming prototype | H.264 slice tuning, validated mode table, and live latency telemetry |
| Application integration controls | AGC reconnect lifecycle, SDR HUD cost, display-owner handoff, source mapping, and inactive-HDR rejection |

The public text is a factual synthesis and the examples were independently
written for this repository under GPL-3.0-or-later. Test harnesses and inputs
remain outside the publication.

## Milestone map

### H.264 and HEVC

| Milestone | Result used by this guide |
|---|---|
| Initial HEVC presentation | One HEVC Main decode and exact-pointer AGC presentation |
| Matched codec controls | H.264/HEVC comparisons at 1080p, 1440p, and 4K |
| Pipeline study | Game-process decoder at depths 1, 3, and 6 |
| WPP control | WPP-enabled controlled 4K depth-three result |
| Media-process control | Alternate scheduling behavior recorded without publishing private setup details |
| Practical 4K60 | Live HEVC at depths 3 and 1 |
| Slice controls | H.264 1080p60 four-slice optimization and eight-slice comparison |
| Reconnect control | AGC initialization is process-global; per-stream resources can still be rebuilt |
| HUD control | Small SDR overlay in the existing AGC submission preserved the measured latency class |
| Display handoff control | Valid decode/AGC markers can coexist with black output when VideoOut ownership transition fails |
| Full-frame mapping diagnosis | Inherited partial-view affine transform cropped 32 left and 16 bottom source pixels |
| Raw-buffer shader audit | Current Y/UV fetches use integer point sampling; filtered scale remains untested |

### HDR and Main10

| Milestone | Result used by this guide |
|---|---|
| Capability control | HDR output was rejected without HDR-capable application metadata |
| Category control | Changing process category alone did not grant HDR |
| Packing control | A2B10G10R10 packed channel order |
| Render control | AGC draw into a 10-bit HDR target |
| Surface control | Two-plane low-aligned 10-bit sampling and preserve-PQ BT.2020-NCL conversion |
| Decode control | HEVC Main10 decode and exact surface storage contract |
| End-to-end control | Same decoded Main10 pointer presented through AGC and public HDR VideoOut |
| Integrated inactive-HDR run | Main10 support/negotiation succeeded while live HDR state remained inactive |
| HDR HUD candidate | Separate same-command-buffer SDR overlay design built, but was not accepted on a live HDR stream |

### AV1

The bounded conclusion came from reviewing the usable Videodec2 interfaces and
codec-specific routes with AVC, HEVC, and VP9 positive controls. No equivalent
AV1 route was identified across the examined firmware-6.02 paths.

### VP9

A bounded firmware-6.02 console run used the public game decoder resource and
a locally generated 1920x1080 Profile 0 keyframe. Memory query, allocation,
create, reset, decode, delete, and queue release all succeeded. Decode accepted
the access unit and returned one valid, error-free VP9 picture at 1920x1080,
with 2048-byte pitch and exact caller-pool pointer identity. No flush was needed.

A matched series decoded 60 frames each at 1080p, 1440p, and 4K with the same
profile, resource, depth, and encoder policy. Every frame was accepted and valid
with exact pointer identity. Post-cold averages were 5.533, 8.617, and
17.407 ms; unpaced batch rates were 173.09, 112.69, and 55.57 FPS.

## Confidence labels

- **Console-proven:** the exact tuple/path ran successfully on firmware 6.02.
- **Controlled only:** a deterministic file or frame ran, but a sustained live
  product workload has not.
- **Platform-interface evidence:** a concrete route or API exists, but this study
  did not execute it.
- **Not proven:** the result must not be inferred from a neighboring mode.
- **Unavailable:** no usable firmware-6.02 implementation path was found.

## Known limitations

- Most benchmark rows are one controlled run, not a distribution across days,
  consoles, firmware revisions, encoders, and games.
- Equal byte rate is useful for isolating decoder behavior but does not measure
  codecs at equal visual quality.
- Remote Play HDR captures are tone-mapped and cannot prove display luminance,
  gamut, or HDMI metadata correctness.
- The AGC render target is tiled/swizzled; CPU-linear pixel address calculations
  are not valid readback coordinates.
- The VideoOut handoff race is the best-supported diagnosis for the observed
  intermittent black output, but the 100 ms mitigation is not a proven API
  requirement.
- The original affine crop is evidence-backed; the corrected mapping had not
  completed a matched live edge-chart acceptance at the recorded milestone.
- The raw-buffer shader's point sampling is source-proven, but no matched
  filtered-versus-point visual/performance run has been completed.
- Main10/HDR is proven for one controlled 1080p frame, not a sustained network
  stream.
- 1440p/4K Main10, B frames, dynamic HDR transitions, live HDR HUD composition,
  and native 4K HDR scanout remain untested.
- 1440p and 4K SDR decoded surfaces were scaled into 1920x1080 VideoOut.
- VP9 Profile 0 has controlled 60-frame decode proofs at 1080p, 1440p, and 4K;
  presentation, Profile 2, deeper pipelines, and tile-layout tuning remain
  untested.
- The AV1 conclusion is firmware/API-specific and does not prove the SoC's
  transistor-level media-engine contents.

## Public primary references

- [Sony PS5 hardware specification](https://blog.playstation.com/archive/2020/03/18/unveiling-new-details-of-playstation-5-hardware-technical-specs/):
  RDNA 2-based GPU, frequency/throughput, system memory, and VideoOut context;
  it does not specify the codec engine.
- [Sony PS5 Media Gallery formats](https://www.playstation.com/en-gb/support/hardware/play-video-music-discs-usb-drives/):
  consumer H.264/VP9 and maximum-resolution support, used only as supporting
  application evidence.
- [AMD supported video formats](https://www.amd.com/en/products/graphics/radeon-for-creators/video-editing.html):
  demonstrates that nearby Radeon products differ in AV1 support, so the RDNA
  generation cannot decide a custom PS5 SoC's exposed codec set.

## Reproducing new measurements

For each materially different candidate:

1. State one hypothesis and one changed variable.
2. Fix the bitstream, executable, runtime, and application metadata for the run.
3. Use a clean authorized test build and exclusive console access.
4. Validate output correctness and caller-pool pointer identity before timing.
5. Record compressed gather, submission call, output-ready, throughput, and
   completed-flip measurements separately.
6. Preserve coded dimensions, visible crop, codec/profile, WPP/slice state,
   bitrate, frame structure, queue depth, and display target in the receipt.
7. Repeat representative live content before promoting a product default.

The 4K and Main10 layout questions were resolved through return codes,
negotiated state, output metadata, and pointer validation; invasive memory
inspection was unnecessary.
