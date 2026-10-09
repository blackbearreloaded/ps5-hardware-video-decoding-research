# Evidence, scope, and reproducibility

## Test boundary

The decoder experiments began on one PlayStation 5 (console A) between
2026-08-24 and 2026-08-29. High-refresh presentation work continued through
2026-08-30 and repeated the true-4K-source control on a second PlayStation 5
(console B) running a different system software version. The work used isolated test applications and
a native real-time streaming prototype. Every successful controlled milestone
fixed the tested build and source, recorded structured results, and shut down
cleanly; decoder milestones additionally validated exact caller-pool pointer
identity.

## Evidence groups

| Evidence group | Contribution retained here |
|---|---|
| H.264 and HEVC console runs | Known-good modes, caller-owned surfaces, resolution comparisons, pipeline depth, WPP, and practical 4K60 timing |
| HDR and Main10 console runs | 10-bit VideoOut, channel packing, BT.2020/PQ conversion, and Main10 surface layout |
| Controlled VP9 console runs | Profile 0/2 lifecycle, resolution scaling, 4K tile-layout, decoder pipeline/recovery controls, caller-owned surface identity, endurance, LAN ingestion, and decode-to-completed-flip presentation |
| Native HFR presenter controls | 1080p 90/120 cadence and true-4K-source 3840x2160/119.88 Hz output on console A and console B |
| Decoder/presenter prototype | Zero-copy pointer identity, point-sampled research output, and later filtered 1440p-to-4K streaming output |
| Real-time streaming prototype | H.264 slice tuning, validated mode table, live latency telemetry, and H.264 1080p/1440p/2160p HFR acceptance |
| Application integration controls | AGC reconnect lifecycle, SDR HUD cost, display-owner handoff, and inactive-HDR rejection |

## Milestone map

### H.264 and HEVC

| Milestone | Result used by this guide |
|---|---|
| Initial HEVC presentation | One HEVC Main decode and exact-pointer AGC presentation |
| Matched codec controls | H.264/HEVC comparisons at 1080p, 1440p, and 4K |
| Pipeline study | Decoder at depths 1, 3, and 6 |
| WPP control | WPP-enabled controlled 4K depth-three result |
| Practical 4K60 | Live HEVC at depths 3 and 1 |
| Slice controls | H.264 1080p60 four-slice optimization and eight-slice comparison |
| Reconnect control | AGC initialization is process-global; per-stream resources can still be rebuilt |
| Process-boundary control | Decoder-to-AGC pointer identity was proven in one process; raw direct-memory offsets did not act as cross-process handles |
| Flip-count control | Removing a redundant second VideoOut flip restored cadence from 29.97 to 59.93 FPS |
| HUD control | Small SDR overlay in the existing AGC submission preserved the measured latency class |
| Display handoff control | Valid decode/AGC markers and even a completed background flip can coexist with invisible output when foreground ownership is elsewhere |
| Raw-buffer shader audit | Original Y/UV fetches use integer point sampling; later filtered 1440p-to-4K streaming presentation was hardware-accepted |

### High refresh and native 4K output

| Milestone | Result used by this guide |
|---|---|
| 1080p HFR cadence | Isolated targets measured 89.99 FPS at 90 FPS and 119.85 FPS at 120 FPS |
| Native 4K HFR, console A | True 3840x2160 source and output; 600/600 frames at 119.87 FPS |
| Native 4K HFR, console B | Same true-4K-source control; 600/600 frames at 119.88 FPS |
| Live sender negotiation | 1440p/90 matched a 90 Hz sender capture; 2160p/120 matched a 120 Hz sender capture |
| Live acceptance | H.264 streaming worked at 1080p/120, 1440p/120, and 2160p/120 |
| HDMI transition control | One TV/port briefly resynchronized before first application draw; another HDMI path did not show the black interval |

The HFR controls prove presentation capacity and the live sessions prove
integration. They are not a controlled cross-codec 4K/120 decode-latency
distribution. See [High-refresh and native 4K output](high-refresh-output.md).

### HDR and Main10

| Milestone | Result used by this guide |
|---|---|
| Packing control | A2B10G10R10 packed channel order |
| Render control | AGC draw into a 10-bit HDR target |
| Surface control | Two-plane low-aligned 10-bit sampling and preserve-PQ BT.2020-NCL conversion |
| Decode control | HEVC Main10 decode and exact surface storage contract |
| End-to-end control | Same decoded Main10 pointer presented through AGC and HDR VideoOut |
| Integrated inactive-HDR run | Main10 support/negotiation succeeded while live HDR state remained inactive |
| HDR HUD candidate | Separate same-command-buffer SDR overlay design built, but was not accepted on a live HDR stream |

### AV1

No AV1 decode is reported. AVC, HEVC, and VP9 were each proven by console
runs; no AV1 conclusion is drawn.

### VP9

A bounded console A run used
a locally generated 1920x1080 Profile 0 keyframe. Memory query, allocation,
create, reset, decode, delete, and queue release all succeeded. Decode accepted
the access unit and returned one valid, error-free VP9 picture at 1920x1080,
with 2048-byte pitch and exact caller-pool pointer identity. No flush was needed.

A matched series decoded 60 frames each at 1080p, 1440p, and 4K with the same
profile, depth, and encoder policy. Every frame was accepted and valid
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

A non-synthetic-video Profile 0 control produced a 287.01 FPS median over three
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

A console B repeat of the decoder benchmarks remained unmeasured.

The native HFR oracles established presentation of a true 3840x2160 source
at 119.88 Hz on console A and console B. Subsequent operator testing accepted
live H.264 2160p/120 through VideoDec2 and AGC, but did not capture a controlled
decode-to-completed-flip latency distribution for that session.

## Confidence labels

- **Console-proven:** the exact tuple/path ran successfully on the stated
  console; decoder rows default to console A unless noted otherwise.
- **Controlled only:** a deterministic file or frame ran, but a sustained live
  workload has not.
- **Not proven:** the result must not be inferred from a neighboring mode.

## Known limitations

- Most benchmark rows are one controlled run, not a distribution across days,
  consoles, system software revisions, encoders, and content.
- Equal byte rate is useful for isolating decoder behavior but does not measure
  codecs at equal visual quality.
- Remote HDR captures are tone-mapped and cannot prove display luminance,
  gamut, or HDMI metadata correctness.
- The AGC render target is tiled/swizzled; CPU-linear pixel address calculations
  are not valid readback coordinates.
- The VideoOut handoff race is the best-supported diagnosis for the observed
  intermittent black output, but the 100 ms mitigation is not a proven API
  requirement.
- The original raw-buffer shader point-samples and the
  later streaming path hardware-accepted filtered 1440p-to-4K output, but no
  matched filtered-versus-point performance distribution was captured.
- HEVC Main10/HDR is proven for one controlled 1080p frame, not a sustained
  network stream.
- VP9 Profile 2 is proven at 1080p and 4K, including completed 4K presentation,
  but representative live HDR content and display-side photometric validation
  remain untested.
- 1440p/4K HEVC Main10, B frames, dynamic HDR transitions, and live HDR HUD
  composition remain untested.
- Most original 1440p/4K decoder benchmarks scaled into 1920x1080 VideoOut.
  Native 4K/119.88 Hz presentation and live H.264 2160p/120 integration are now
  proven separately, but a controlled multi-codec decoder-to-native-scanout
  benchmark is still missing.
- VP9 Profile 0/2 results use a small number of synthetic and non-synthetic streams;
  tile count, throughput, and latency policy are not universal encoder claims.
- Console B portability is proven for the true-4K-source 119.88 Hz
  presenter and streaming lifecycle, not for every decoder benchmark.

## References

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
2. Fix the bitstream, executable, and runtime for the run.
3. Use a clean test build and exclusive console access.
4. Validate output correctness and caller-pool pointer identity before timing.
5. Record compressed gather, submission call, output-ready, throughput, and
   completed-flip measurements separately.
6. Preserve coded dimensions, visible crop, codec/profile, WPP/slice state,
   bitrate, frame structure, queue depth, and display target in the receipt.
7. Repeat representative live content before promoting a default.

The 4K and Main10 layout questions were resolved through return codes,
negotiated state, output metadata, and pointer validation.
