# PS5 Hardware Video Decoding Research

[![Examples](https://github.com/blackbearreloaded/ps5-hardware-video-decoding-research/actions/workflows/examples.yml/badge.svg)](https://github.com/blackbearreloaded/ps5-hardware-video-decoding-research/actions/workflows/examples.yml)
[![PS5 firmware 6.02](https://img.shields.io/badge/PS5_firmware-6.02-003791.svg)](docs/evidence.md)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

Independent, console-validated interoperability research into low-latency H.264,
HEVC, VP9, Main10/HDR, resolution scaling, zero-copy presentation, and performance
measurement on PlayStation 5.

The public game-process `libSceVideodec2` path decodes H.264 High and HEVC Main
at 1080p, 1440p, and 4K into caller-owned GPU-visible memory. Controlled runs
also prove VP9 Profile 0 decode at those resolutions to caller-owned direct memory.
AGC consumes the exact returned pointer for the validated AVC/HEVC presentation
paths, eliminating decoded-frame CPU copies. A bounded 1080p experiment also
proves HEVC Main10 decode through public HDR VideoOut.

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
| VP9 Profile 0 | Controlled decode: 173.09 FPS at 1080p, 112.69 at 1440p, and 55.57 at 4K; presentation untested |
| AV1 | No usable firmware-6.02 decoder path found through the examined interfaces |
| Native 4K scanout | Not tested; 4K decoded surfaces were scaled to 1920x1080 VideoOut |
| CI | Builds and runs the host-side contract examples |

Firmware interfaces and title capabilities can change. Treat firmware 6.02 as
the verified baseline, and keep controlled, live-product, firmware-inferred,
and untested claims separate.

## Research highlights

| Finding | Result |
| --- | --- |
| Shared-memory optimization | Videodec2 and AGC use the same caller-owned direct-memory surface |
| Per-frame CPU copy | Only the compressed access-unit gather remains; measured at roughly 1–5 us |
| H.264 1080p optimization | Four slices reduced average Videodec2 time from 4.761 ms to 2.268 ms |
| HEVC practical 4K60 | Depth one averaged 5.464 ms decode and 16.698 ms callback-to-completed-flip |
| HEVC WPP | Controlled depth-three 4K throughput improved 36.2% |
| Pipeline depth | More frames in flight improve throughput but increase frame residency |
| Main10 storage | Two-plane 4:2:0 with low-aligned 10-bit words, not MSB-aligned P010 |
| HDR output | BT.2020-NCL conversion preserves PQ into A2B10G10R10 VideoOut |
| Reconnect lifecycle | AGC initialization is process-global; retain it while rebuilding per-stream resources |
| In-band SDR HUD | One extra draw in the existing AGC command buffer caused no meaningful measured latency regression |
| Source mapping | An inherited partial-view transform cropped 32 left and 16 bottom pixels despite valid full-frame decode |
| Sampling quality | The current raw-buffer shader point-samples luma/chroma; filtered downscaling remains untested |

## Proven data path

```text
Compressed network stream
  -> complete encoded access unit
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
| VP9 Profile 0 | Controlled console proof | 8-bit 4:2:0 at 1080p, 1440p, and 2160p; caller-owned direct-memory output |
| AV1 | Unavailable through examined APIs | No usable decoder route was identified |
| Native 4K scanout | Not proven | Current 4K tests scale into a 1920x1080 display target |

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
| Controlled VP9 Profile 0 4K60 source | Depth 1, decode-only | 17.407 ms steady decode; 55.57 FPS batch throughput |

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
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/video_modes.cpp -o build/video_modes
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/hdr_contract.cpp -o build/hdr_contract
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/timing_model.cpp -o build/timing_model

./build/video_modes h264 1080
./build/video_modes hevc 2160
./build/video_modes main10 1080
./build/hdr_contract
./build/timing_model
```

See [Minimal examples](examples/README.md) for the use-case mapping.

## Implementation guidance

1. Use the game-process decoder resource and pipeline depth `1` by default.
2. Query decoder memory at runtime, align allocations to 16 KiB, and rotate
   fixed compressed-input and caller-owned frame pools.
3. Request four H.264 slices at 1080p; measure other counts per resolution.
4. Request HEVC WPP and verify the PPS instead of assuming it is enabled.
5. Preserve coded dimensions separately from visible crop, particularly
   3840x2176 coded versus 3840x2160 visible.
6. Keep H.264, HEVC Main, and Main10/HDR as explicit negotiated modes.
7. Measure submission, output-ready, throughput, and completed-flip boundaries
   separately.

## Documentation

| Document | Purpose |
| --- | --- |
| [Architecture and memory](docs/architecture.md) | Videodec2, AGC, VideoOut, memory ownership, zero-copy limits, and lifecycle |
| [Codecs and resolutions](docs/codecs-and-resolutions.md) | Proven codec/profile/level tuples, pitches, coded sizes, and visible crops |
| [Benchmarks](docs/benchmarks.md) | Controlled and live timing tables, pipeline depth, WPP, slices, and methodology |
| [HDR and Main10](docs/hdr.md) | Title capability, decoded storage, BT.2020/PQ conversion, packing, and negotiation |
| [Implementation guide](docs/implementation.md) | Minimal real-time streaming integration, validation, telemetry, errors, and rollout |
| [Evidence and limits](docs/evidence.md) | Research snapshots, milestone map, confidence labels, and unproven areas |
| [Examples](examples/README.md) | Host-compilable codec, resolution, HDR, and timing contract checks |
| [Publication policy](PUBLICATION.md) | Content boundary, contributor checklist, and legal-review limits |

## Repository layout

```text
README.md                         Research overview and headline results
docs/architecture.md              Hardware stages and memory ownership
docs/codecs-and-resolutions.md    Decoder tuples and output layouts
docs/benchmarks.md                Complete controlled and live measurements
docs/hdr.md                       Main10 and public HDR presentation contract
docs/implementation.md            Real-time streaming integration guidance
docs/evidence.md                  Evidence lineage and research limits
examples/video_modes.cpp          Codec/resolution mode selection
examples/hdr_contract.cpp         Main10 layout, color matrix, and packing
examples/timing_model.cpp         Submission, ready, and flip timing semantics
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

## External projects and references

| Project or reference | Role |
| --- | --- |
| [FFmpeg](https://ffmpeg.org/) | Controlled H.264/HEVC asset generation and bitstream inspection |
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
