# PS5 Hardware Video Decoding Research

[![Examples](https://github.com/blackbearreloaded/ps5-hardware-video-decoding-research/actions/workflows/examples.yml/badge.svg)](https://github.com/blackbearreloaded/ps5-hardware-video-decoding-research/actions/workflows/examples.yml)
[![PS5 firmware 6.02](https://img.shields.io/badge/PS5_firmware-6.02-003791.svg)](docs/evidence.md)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

Independent, console-validated interoperability research into low-latency H.264,
HEVC, VP9, Main10/HDR, resolution scaling, zero-copy presentation, and performance
measurement on PlayStation 5.

The public game-process `libSceVideodec2` path decodes H.264 High and HEVC Main
at 1080p, 1440p, and 4K into caller-owned GPU-visible memory. Controlled runs
also prove VP9 Profile 0 decode at those resolutions and Profile 2 at 1080p and
4K, including tiled 4K and correctly drained depth-three pipelines, to caller-
owned direct memory.

AGC consumes the exact returned pointer for the validated AVC/HEVC/VP9
presentation paths, eliminating decoded-frame CPU copies. A bounded 1080p
experiment also proves HEVC Main10 decode through public HDR VideoOut.

This repository uses **hardware video decoding** deliberately. Videodec2 is the
console's media-decoder path; the results do not imply that RDNA shader compute
units execute the codecs. AGC is the GPU stage that samples decoded surfaces,
converts color, composites, scales, and renders into VideoOut framebuffers.

## Project status

| Area | Status |
| --- | --- |
| Verified platform | PlayStation 5, firmware 6.02 |
| H.264 / AVC High | Console-proven at 1080p, 1440p, and 2160p |
| H.265 / HEVC Main | Console-proven at 1080p, 1440p, and 2160p |
| HEVC Main10 / HDR10 | One controlled 1080p frame proven end to end |
| Zero-copy presentation | Exact Videodec2 output pointer consumed by AGC |
| Practical HEVC 4K60 | 60.36 FPS at depth one; 5.464 ms average synchronous decode |
| VP9 Profile 0 | Controlled decode at 1080p, 1440p, and 4K; four-tile 4K reached 187.39 FPS decode-only and 59.95 FPS through serialized completed flips |
| VP9 Profile 2 | Caller-owned low-aligned 10-bit output at 1080p/4K; four-tile 4K reached 170.51 FPS decode-only and completed 60 Hz HDR-target presentation |
| AV1 | No usable firmware-6.02 decoder path found through the examined interfaces |
| Native 4K scanout | Standard 3840x2160 VideoOut and about 59.9 FPS HDR presentation proven independently; that control used software decoding |
| CI | Builds and runs the host-side contract examples |

Firmware interfaces and title capabilities can change. Treat firmware 6.02 as
the verified baseline, and keep controlled, live-product, firmware-inferred,
and untested claims separate.

## Research highlights

| Finding | Result |
| --- | --- |
| Shared-memory optimization | Videodec2 and AGC use the same caller-owned direct-memory surface |
| Zero-copy process boundary | Exact-pointer reuse is proven within one process; a raw direct-memory offset is not a portable cross-process handle |
| Per-frame CPU copy | Only the compressed access-unit gather remains; measured at roughly 1–5 us |
| H.264 1080p optimization | Four slices reduced average Videodec2 time from 4.761 ms to 2.268 ms |
| HEVC practical 4K60 | Depth one averaged 5.464 ms decode and 16.698 ms callback-to-completed-flip |
| HEVC WPP | Controlled depth-three 4K throughput improved 36.2% |
| Pipeline depth | More frames in flight improve throughput but increase frame residency |
| VP9 4K tiling | Four tile columns improved depth-one throughput from 55.57 to 79.10 FPS with only 0.06% more encoded data |
| VP9 4K presentation | Exact-pointer two-plane presentation sustained 59.95 FPS steady, with 16.676 ms average AU-ready-to-completed-flip latency |
| VP9 Profile 2 | 4K depth one reached 72.37 FPS; depth three reached 170.51 FPS and completed 60/60 10-bit presentations at paced 59.53 FPS |
| VP9 endurance | Natural-content depth-three median was 287.01 FPS; 36,000 paced 4K60 frames completed with no full-frame deadline misses |
| VP9 LAN ingestion | 60/60 framed 4K Profile 2 AUs completed; serialized receive exposed backlog and supports a separate bounded network producer |
| VP9 frame structure | Split compound superframes; submit hidden frames without presenting; show-existing returns a materialized caller-owned output |
| 10-bit storage | HEVC Main10 and VP9 Profile 2 use two-plane 4:2:0 with low-aligned words, not MSB-aligned P010 |
| HDR output | BT.2020-NCL conversion preserves PQ into A2B10G10R10 VideoOut |
| Reconnect lifecycle | AGC initialization is process-global; retain it while rebuilding per-stream resources |
| Flip ownership | Use AGC's integrated flip once; a redundant second flip reduced a controlled 60 FPS path to about 30 FPS |
| Foreground ownership | A successful background flip did not transfer compositor visibility to that process |
| In-band SDR HUD | One extra draw in the existing AGC command buffer caused no meaningful measured latency regression |
| Source mapping | An inherited partial-view transform cropped valid pixels; restoring the complete affine and destination safe-area state passed a controlled full-frame acceptance |
| Sampling quality | The current raw-buffer shader point-samples luma/chroma; filtered downscaling remains untested |

## Proven data path

```text
Compressed network stream
  -> framed container/protocol packet
  -> split VP9 compound packets into individual coded frames when present
  -> complete encoded access unit
  -> bounded receive/jitter queue independent of blocking presentation
  -> ordered fragment gather into a fixed direct-memory input slot
  -> Videodec2 decode into a caller-owned frame slot
  -> codec, coded-size, pitch, byte-pitch, and pointer validation
  -> same pointer bound as AGC Y/UV textures
  -> color conversion, composition, and scaling
  -> VideoOut framebuffer and completed flip
```

“Zero-copy” refers specifically to decoder-to-AGC pointer identity. Compressed
fragments still require a small contiguous gather, and AGC still reads and
renders the surface into a scanout framebuffer.

## Capability matrix

| Codec or feature | Evidence | Proven scope |
| --- | --- | --- |
| H.264 / AVC High | Console-proven | 8-bit 4:2:0 at 1080p, 1440p, and 2160p |
| H.265 / HEVC Main | Console-proven | 8-bit 4:2:0 at 1080p, 1440p, and 2160p |
| H.265 / HEVC Main10 | Controlled console proof | 1920x1080 BT.2020/PQ frame and caller-owned 10-bit surface |
| HDR10 presentation | Controlled console proof | Main10 -> AGC BT.2020-NCL -> 10-bit HDR VideoOut |
| VP9 Profile 0 | Controlled console proof | 8-bit 4:2:0 at 1080p, 1440p, and 2160p; caller-owned direct-memory output and 4K scaled presentation |
| VP9 Profile 2 | Controlled console proof | Low-aligned 10-bit 4:2:0 at 1080p and 2160p; caller-owned output and 4K HDR-target presentation |
| AV1 | Unavailable through examined APIs | No usable decoder route was identified |
| Native 4K scanout | Supporting full-player proof | Standard 3840x2160 VideoOut at about 59.9 FPS; software decoder, so not a Videodec2 benchmark |

The AV1 result is an API and firmware conclusion. It does not prove the custom
SoC physically lacks every possible AV1-capable circuit.

## Benchmark snapshot

All rows are single-console results, not confidence intervals. Codec profile,
bitrate, content complexity, slices, WPP, queue depth, and measurement boundary
must match before comparing platforms.

| Workload | Policy | Result |
| --- | --- | --- |
| Live network H.264 1080p60 | Four slices, depth 1 | 2.268 ms decode + 0.096 ms flush; 60.00 FPS |
| Live network HEVC 4K60, about 12 Mb/s | Depth 1, WPP off | 5.464 ms decode; 5.472 ms ready; 16.698 ms callback-to-flip; 60.36 FPS |
| Same live HEVC stream | Depth 3, WPP off | 0.333 ms submission; 33.710 ms ready; 50.059 ms callback-to-flip; 60.10 FPS |
| Controlled HEVC 4K, about 72 Mb/s | Depth 1, WPP off | 20.719 ms synchronous decode; 47.94 FPS batch throughput |
| Same controlled bytes | Depth 3, WPP off | 22.993 ms ready; 128.39 FPS batch throughput |
| Controlled HEVC 4K | Depth 3, WPP on | 16.601 ms ready; 174.89 FPS batch throughput |
| Controlled VP9 Profile 0 1080p60 | Depth 1, decode-only | 5.533 ms steady decode; 173.09 FPS batch throughput |
| Controlled VP9 Profile 0 1440p60 | Depth 1, decode-only | 8.617 ms steady decode; 112.69 FPS batch throughput |
| Controlled VP9 Profile 0 4K60 source | Single tile, depth 1, decode-only | 17.407 ms steady decode; 55.57 FPS batch throughput |
| Controlled VP9 Profile 0 4K60 source | Four tiles, depth 1, decode-only | 12.376 ms steady decode; 79.10 FPS batch throughput |
| Same four-tile VP9 bytes | Depth 3, decode-only | 4.633 ms median submission; 15.082 ms median ready; 187.39 FPS batch throughput |
| Same four-tile VP9 stream | Depth 1, serialized decode/present | 12.311 ms decode/ready; 16.676 ms AU-ready-to-completed-flip; 59.95 FPS steady |
| Controlled VP9 Profile 2 4K | Four tiles, depth 1 / 3 | 72.37 / 170.51 FPS decode-only batch throughput |
| Same Profile 2 stream | Depth 3, paced HDR-target presentation | 0.401 ms submission; 40.483 ms AU-ready-to-flip; 59.53 FPS |
| Natural-content VP9 Profile 0 4K | Four tiles, depth 3, three runs | 287.01 FPS median; 3.6% max-to-min spread |
| Same natural-content policy | Ten-minute paced endurance | 36,000 frames; zero full-frame deadline misses |
| Paced LAN VP9 Profile 2 4K | Serialized receive/decode/present control | 0.266 ms AU fill; 50.066 ms AU-ready-to-flip; 60/60 complete |

The 0.333 ms depth-three figure is API submission occupancy, not decode
latency. See [Benchmarks and measurement method](docs/benchmarks.md) for the
complete controlled/live tables and timing definitions.

## Quick start

The examples are standard C++20 contract checks. They require no Sony SDK,
firmware, proprietary headers, or PS5 toolchain.

```sh
git clone git@github.com:blackbearreloaded/ps5-hardware-video-decoding-research.git
cd ps5-hardware-video-decoding-research

mkdir -p build
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/h264/modes.cpp -o build/h264_modes
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/hevc/modes.cpp -o build/hevc_modes
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/vp9/modes.cpp -o build/vp9_modes
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/vp9/packetization.cpp -o build/vp9_packetization
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/hdr/surface_contract.cpp -o build/hdr_surface_contract
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/common/memory_pipeline.cpp -o build/memory_pipeline
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/common/timing_model.cpp -o build/timing_model

./build/h264_modes high 1080
./build/hevc_modes main 2160
./build/hevc_modes main10 1080
./build/vp9_modes profile2 2160
./build/hdr_surface_contract
./build/memory_pipeline
./build/timing_model
./build/vp9_packetization
```

See [Minimal examples](examples/README.md) for the use-case mapping.

## Implementation guidance

1. Use the game-process decoder resource and pipeline depth `1` by default.
2. Query decoder memory at runtime, align allocations to 16 KiB, and rotate
   fixed compressed-input and caller-owned frame pools.
3. Request four H.264 slices at 1080p; measure other counts per resolution.
4. Request HEVC WPP and verify the PPS instead of assuming it is enabled.
5. Request four VP9 tile columns for the tested 4K encoder policy, then verify
   the bitstream and measure representative live content.
6. Split VP9 compound superframes into coded frames; submit hidden frames but
   suppress presentation, and present materialized show-existing output.
7. Keep network receive/jitter buffering independent of blocking completed-flip
   presentation, with a bounded queue and explicit backpressure.
8. Preserve coded dimensions separately from visible crop, particularly
   3840x2176 coded versus 3840x2160 visible.
9. Keep H.264, HEVC Main/Main10, and VP9 Profile 0/2 as explicit negotiated modes.
10. Measure network first-byte, AU-ready, submission, output-ready, throughput,
    and completed-flip boundaries separately.

## Documentation

| Document | Purpose |
| --- | --- |
| [Architecture and memory](docs/architecture.md) | Videodec2, AGC, VideoOut, memory ownership, zero-copy limits, and lifecycle |
| [Codecs and resolutions](docs/codecs-and-resolutions.md) | Proven codec/profile/level tuples, pitches, coded sizes, and visible crops |
| [Benchmarks](docs/benchmarks.md) | Controlled and live timing tables, pipeline depth, WPP, slices, and methodology |
| [10-bit surfaces and HDR10](docs/hdr.md) | HEVC Main10/VP9 Profile 2 storage, title capability, BT.2020/PQ conversion, packing, and negotiation |
| [Implementation guide](docs/implementation.md) | Minimal real-time streaming integration, validation, telemetry, errors, and rollout |
| [Evidence and limits](docs/evidence.md) | Research snapshots, milestone map, confidence labels, and unproven areas |
| [Examples](examples/README.md) | Host-compilable codec, resolution, HDR, timing, and VP9 packetization contract checks |
| [Publication policy](PUBLICATION.md) | Content boundary, contributor checklist, and legal-review limits |

## Repository layout

```text
README.md                         Research overview and headline results
docs/architecture.md              Hardware stages and memory ownership
docs/codecs-and-resolutions.md    Decoder tuples and output layouts
docs/benchmarks.md                Complete controlled and live measurements
docs/hdr.md                       HEVC/VP9 10-bit and HDR presentation contract
docs/implementation.md            Real-time streaming integration guidance
docs/evidence.md                  Evidence lineage and research limits
examples/h264/modes.cpp            H.264 mode and surface selection
examples/hevc/modes.cpp            HEVC Main/Main10 mode and surface selection
examples/vp9/modes.cpp             VP9 Profile 0/2 mode and surface selection
examples/vp9/packetization.cpp     Superframe and presentation policy
examples/hdr/surface_contract.cpp  10-bit layouts, color matrix, and packing
examples/common/memory_pipeline.cpp Aligned pools, gather, pointer reuse, and fences
examples/common/timing_model.cpp   Submission, ready, and flip timing semantics
examples/common/video_mode.hpp     Shared mode validation and reporting
.github/workflows/examples.yml    Strict C++20 example validation
PUBLICATION.md                    Publication and contribution boundary
```

## Methodology

Every console milestone changed one bounded variable, fixed the tested build
and bitstream, validated output metadata and caller-pool pointer identity,
recorded structured results, and completed the resource lifecycle.

Evidence labels are intentionally narrow:

- **Console-proven:** the exact tuple or path ran successfully.
- **Controlled only:** a deterministic asset ran, but sustained live use did
  not.
- **Platform-interface evidence:** a concrete route or API exists but was not
  exercised here.
- **Unavailable:** no usable firmware-6.02 implementation path was found.
- **Bounded unavailable:** a prerequisite failed repeatedly before the target
  behavior could be measured; no positive or negative capability is inferred.

## External projects and references

| Project or reference | Role |
| --- | --- |
| [FFmpeg](https://ffmpeg.org/) | Controlled H.264/HEVC/VP9 asset generation and bitstream inspection |
| [x265](https://bitbucket.org/multicoreware/x265_git/wiki/Home) | HEVC encoder used for controlled WPP comparisons |
| [Sony PS5 specifications](https://blog.playstation.com/archive/2020/03/18/unveiling-new-details-of-playstation-5-hardware-technical-specs/) | Public GPU, memory, and VideoOut context |
| [AMD video formats](https://www.amd.com/en/products/graphics/radeon-for-creators/video-editing.html) | Evidence that RDNA-family products do not share one universal codec matrix |

## Scope

This repository publishes independently written documentation and small clean
examples. It does not contain Sony SDK files, firmware modules, retail
application binaries, implementation extracts, signing material, credentials,
decoded frames, or proprietary test assets. It does not provide access-control
bypass instructions, alter console settings, or claim compatibility beyond the
tested firmware and workloads.

See [PUBLICATION.md](PUBLICATION.md) for the publication boundary and
[NOTICE.md](NOTICE.md) for project and trademark attribution.

## License and attribution

Repository-authored documentation and examples are licensed under
GPL-3.0-or-later. See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).

This project was developed with assistance from OpenAI Codex. Project
maintainers reviewed and validated the resulting documentation and examples.

PlayStation and PS5 are trademarks of Sony Interactive Entertainment. This
project is independent and is not affiliated with or endorsed by Sony.
