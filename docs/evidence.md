# Evidence, scope, and reproducibility

## Test boundary

The console experiments ran on one PlayStation 5 with firmware 6.02 between
2026-08-24 and 2026-08-29. They used isolated, authorized test applications and a native
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
| Controlled VP9 console runs | Profile 0/2 lifecycle, resolution scaling, 4K tile-layout, decoder pipeline/recovery controls, caller-owned surface identity, endurance, LAN ingestion, and decode-to-completed-flip presentation |
| Independent full-player control | Native 3840x2160 VideoOut and 4K60 HDR presentation capacity, kept separate from its software decoder |
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
| Full-frame mapping diagnosis | Inherited partial-view affine transform cropped valid pixels; a complete affine and destination safe-area control restored the full chart |
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

A matched 4K control changed only tile layout. Four tile columns raised
depth-one throughput from 55.57 to 79.10 FPS with a 0.06% encoded-size increase.
The exact four-tile bytes were then decoded at depth three: 58 outputs arrived
during submission, two during drain, and the complete batch sustained
187.39 FPS. Median submission duration was 4.633 ms; median output-ready
latency was 15.082 ms and p95 was 21.571 ms.

The same four-tile stream was then presented at serialized depth one through
the tested linear 8-bit two-plane path. The complete controlled chart was
visible, every reported decoder and presentation stage succeeded, and the
submitted flip marker completed. Frames 1 through 59 averaged 16.676 ms from
AU ready to completed flip and sustained 59.95 FPS. This timing starts after
network reception and access-unit assembly; it is not a live callback result.

Profile 2 controls proved low-aligned 10-bit caller-owned output at 1080p and
4K. The matched four-tile 4K pair reached 72.37 FPS at depth one and 170.51 FPS
at depth three. An ordered three-slot pipeline presented all 60 4K outputs
through the 10-bit target at 59.53 FPS paced cadence, with 0.401 ms average
submission and 40.483 ms average AU-ready-to-completed-flip latency. The remote
capture did not add pixel-level visual evidence for this 4K run.

A natural-content Profile 0 control produced a 287.01 FPS median over three
runs. Its ten-minute, 36,000-frame extension had zero full-frame deadline
misses. A separate paced TCP control received every one of 7,315,255 payload
bytes and completed 60/60 decode/present operations. The intentionally
serialized receive/decode/present design accumulated 85.917 ms of receive
backlog, directly supporting a separate bounded network producer.

Frame-structure controls established that compound superframe packets must be
split into coded frames for this API, hidden frames still produce outputs but
must not be presented, and show-existing commands materialize output into the
new caller slot. Malformed input was recoverable with reset and a new keyframe;
keyframe-driven 1080p/4K resolution changes succeeded within a 4K-configured
decoder.

The alternate decoder resource could be queried but decoder creation was not
available, so no resource-class performance comparison exists. A firmware
12.70 repeat remained unmeasured because the target's deployment service was
unavailable on two locked attempts before application launch.

Independent full-player evidence established native 3840x2160 VideoOut and
about 59.9 FPS 4K60 HDR presentation. That player used software decoding, so
the result is only scanout/presentation evidence.

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
- The original affine crop and controlled corrected mapping are evidence-backed;
  representative live-content acceptance remains untested.
- The raw-buffer shader's point sampling is source-proven, but no matched
  filtered-versus-point visual/performance run has been completed.
- HEVC Main10/HDR is proven for one controlled 1080p frame, not a sustained
  network stream.
- VP9 Profile 2 is proven at 1080p and 4K, including completed 4K presentation,
  but representative live HDR content and display-side photometric validation
  remain untested.
- 1440p/4K HEVC Main10, B frames, dynamic HDR transitions, and live HDR HUD
  composition remain untested.
- Most 1440p/4K decoder runs scaled into 1920x1080 VideoOut. Native 4K VideoOut
  is supported by separate software-player evidence, not a matched hardware-
  decoder-to-native-scanout benchmark.
- VP9 Profile 0/2 results use a small number of synthetic and natural streams;
  tile count, throughput, and latency policy are not universal encoder claims.
- Firmware 12.70 portability is not measured.
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
