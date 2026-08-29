# Benchmarks and measurement method

## Do not compare unlike timings

“Decode time” can refer to different boundaries. This research keeps four
primary measurements separate:

| Measurement | Start | End | What it means |
|---|---|---|---|
| Submission-call duration | Immediately before `sceVideodec2Decode` | API return | CPU/API occupancy; equals ready latency only for a synchronous depth-one call |
| Submission-to-output-ready | Submission/callback timestamp | Call that returns the correlated output | Per-frame decoder pipeline residency |
| Sustained decode-only throughput | First batch submission | Final drained output | Capacity with multiple frames in flight |
| Callback-to-completed-flip | Encoded-frame callback arrival | Submitted VideoOut marker observed complete | Gather + decode residency + AGC + display pacing |

Compressed gather and presenter duration are useful secondary metrics.
Presenter duration includes waiting for the observed flip marker, so it is not
an isolated shader microbenchmark.

At pipeline depth one, submission duration and output-ready latency are nearly
the same. At depth three, a 0.333 ms submission can correspond to 33.710 ms
until that frame is ready. Reporting only the first number would be misleading.

## Matched H.264 versus HEVC

These controlled streams used the game-process decoder resource, pipeline depth
one, progressive 8-bit 4:2:0, and serialized decode/present cadence. Values are
steady averages after excluding the cold first decode. H.264 includes its
average immediate flush because the tested H.264 streams needed one and the
HEVC streams did not.

| Matched workload | Approx. encoded rate | HEVC decode | H.264 decode + flush | HEVC relative to H.264 | Serialized cadence |
|---|---:|---:|---:|---:|---:|
| 1080p low entropy | 0.29 Mb/s | 2.554 ms | 1.621 ms | 57.56% slower | 59.93 FPS |
| 1080p `testsrc2` | 18 Mb/s | 5.841 ms | 6.396 ms | 8.68% faster | 59.94 FPS |
| 1440p `testsrc2` | 31.6 Mb/s | 9.872 ms | 9.405 ms | 4.97% slower | 59.93–59.94 FPS |
| 4K `testsrc2` | 71.7 Mb/s | 20.660 ms | 19.269 ms | 7.22% slower | 29.96–29.97 FPS |

The codec winner changes with content. HEVC is not intrinsically cheaper to
decode on this path. Its product value is generally better compression at a
quality target, which this equal-rate control deliberately does not measure.

The approximately 2 ms H.264 observation is consistent with these results:
low-complexity and slice-parallel live streams can be near that value, while a
high-entropy matched stream can take materially longer.

## Controlled VP9 1080p60

A locally generated 60-frame VP9 Profile 0 `testsrc2` stream was decoded with
the public game resource at pipeline depth one, without pacing or presentation.
Every access unit was accepted and returned one valid, error-free 1920x1080
picture in the caller-owned frame allocation. Decode returned output directly;
no flushes were required.

| Frames | Encoded bytes | Cold call | Steady average | Steady range | Whole-batch throughput |
|---:|---:|---:|---:|---:|---:|
| 60 | 1,854,464 | 19.921 ms | 5.533 ms | 4.892–6.726 ms | 173.09 FPS |

The batch took 346.631 ms and includes compressed-payload copies and loop
overhead. The steady average excludes the first call. This establishes ample
headroom for this controlled 1080p60 stream, but it is not a matched codec
comparison and does not include rendering, display pacing, or network latency.

## H.264 live slice tuning at 1080p60

All three runs used the same depth-one live architecture. The four-slice
capability asks the host encoder to divide a frame for decoder parallelism; it
is not direct submission from the network callback. Direct submission remains
disabled because the presenter waits for a completed flip and therefore is not
non-blocking.

| Host H.264 policy | Videodec2 average | Immediate flush | Callback-to-decode | Callback-to-flip | Delivered cadence |
|---|---:|---:|---:|---:|---:|
| Single slice | 4.761 ms | not separately compared here | 4.861 ms | 16.686 ms | 60.03 FPS |
| Four slices | 2.268 ms | 0.096 ms | 2.372 ms | 16.684 ms | 60.00 FPS |
| Eight slices | 2.069 ms | — | — | 16.681 ms | 59.89 FPS |

Four slices cut the core decode average by 52.4% relative to the single-slice
run. Eight slices saved only 0.199 ms more, increased compressed bytes and
network fragments by about 9%, and slightly reduced delivered cadence. Four
slices are therefore the evidence-backed 1080p default. Higher resolutions
should be measured independently before reusing that count.

The display number stayed near one 60 Hz interval. The decode improvement adds
headroom and reduces risk; it cannot make a vblank-paced completed flip occur
in 2 ms.

## Renderer integration controls

Two application-integration experiments changed renderer behavior without
changing the decoder mode:

| Control | Frames | Videodec2 average | Callback-to-flip average | Result |
|---|---:|---:|---:|---|
| Four-slice H.264 baseline, no HUD | 3,700 | 2.268 ms | 16.684 ms | Reference |
| Same architecture with in-band SDR HUD | 1,661 | 2.312 ms | 16.689 ms | No meaningful regression in this bounded comparison |

The HUD regenerated a small 8-bit NV12 text surface every 15 frames and drew it
after the video quad in the existing AGC command buffer. It added one draw, but
no decoded-surface copy, CPU readback, or second GPU submission. The 0.005 ms
callback-to-flip difference is smaller than these single-run experiments can
attribute confidently; this is evidence that the design preserved the latency
class, not a claim that the extra draw costs exactly 0.005 ms.

A separate reconnect test confirmed that AGC must be initialized once per
process. After retaining the initial state, two consecutive sessions presented
909/909 and 553/553 frames at 13.515 ms and 13.502 ms average
callback-to-completed-flip latency. Reinitializing AGC had previously allowed
the second session to decode 671 access units but present none.

Finally, a UI-renderer-to-AGC VideoOut handoff mitigation was followed by four
visible cycles at 59.89, 60.18, 60.23, and 59.94 FPS. Decode remained about
2.29 ms and callback-to-flip about 16.7 ms. That control isolates renderer
ownership from decoder performance; it does not establish a universal 100 ms
handoff requirement.

## HEVC 4K pipeline-depth and WPP experiment

The controlled input was a deterministic 30-frame 3840x2160 `testsrc2` HEVC
Main Level 5.1 stream at roughly 72 Mb/s with no B frames. Except for the WPP
row, the bytes were identical. These averages include the cold call because
the queue-depth study compared complete batches.

| Resource | Depth | WPP | Submission avg | Output-ready avg | Whole-batch throughput | Interpretation |
|---|---:|---:|---:|---:|---:|---|
| Game process | 1 | Off | 20.719 ms | 20.719 ms | 47.94 FPS | Synchronous reference |
| Game process | 3 | Off | 7.462 ms | 22.993 ms | 128.39 FPS | Throughput headroom, more residency |
| Game process | 6 | Off | 4.551 ms | 32.570 ms | 176.91 FPS | Highest tested throughput, still more residency |
| Game process | 3 | On | 5.212 ms | 16.601 ms | 174.89 FPS | Strong host bitstream optimization |
| Non-public media process | 1 | Off | 10.469 ms | 10.469 ms | 94.06 FPS | Research control only; not integration guidance |
| Non-public media process | 5 | Off | 9.044 ms | 50.303 ms | 94.05 FPS | No throughput gain; large latency cost |

Public depth three raised batch throughput 2.68x over depth one without making
individual frames ready sooner. Public depth six reached 3.69x throughput but
raised average ready latency another 9.577 ms over depth three.

The WPP-enabled stream was 1.17% larger yet improved public depth-three batch
throughput by 36.2%, reduced submission occupancy by 30.2%, and reduced ready
latency by 27.8%. Its PPS explicitly had
`entropy_coding_sync_enabled_flag = 1`; do not assume an encoder preset enabled
WPP without checking the bitstream.

The non-public media-process control demonstrated a different scheduling
contract, but it did not scale with a deeper queue and is intentionally not
documented as an integration path. The supported game-process route remains
the recommendation.

## Practical live HEVC 4K60

The host encoder was asked for HEVC Main, 3840x2160 at 60 FPS and a 150 Mb/s
cap. The actual payload was about 12 Mb/s, no B frames, and WPP off. AGC scaled
the 4K decoded surface into the existing 1920x1080 VideoOut target.

| Measurement | Depth 1 | Depth 3 |
|---|---:|---:|
| Access units entering decoder | 725 | 724 |
| Frames reaching completed flip | 725 | 722; two remained queued at timed shutdown |
| Cadence | 60.36 FPS | 60.10 FPS |
| Compressed gather average | 0.005 ms | 0.005 ms |
| Decode submission average | 5.464 ms | 0.333 ms |
| Submission/callback to output-ready average | 5.472 ms | 33.710 ms |
| Presenter average, including flip observation | 11.223 ms | 16.346 ms |
| Callback to completed flip average | 16.698 ms | 50.059 ms |
| Callback to completed flip maximum | 34.455 ms | 61.250 ms |

Depth one sustained the same practical cadence while reducing ready latency by
83.77% and callback-to-flip latency by 66.64%. It is the correct low-latency
default for this workload. Depth three is an opt-in headroom policy for a
stream whose depth-one percentiles threaten 16.67 ms.

Do not compare the live 5.464 ms and controlled 20.719 ms results without the
approximately 12 versus 72 Mb/s payload and content-complexity difference.

## Why mini-PC and media-box numbers can look lower

Published decoder figures often measure API submission, asynchronous GPU/media
queue occupancy, or decode-only throughput rather than submission-to-ready or
presented latency. They may also use lower-complexity streams, deeper queues,
more parallel slices/tiles, different power/resource classes, or omit vblank.

The PS5 results reproduce the same phenomenon: depth-three submission was only
0.333 ms, but the frame was ready after 33.710 ms. Conversely, the practical
depth-one 4K stream really did decode synchronously in 5.464 ms. A platform
comparison is meaningful only when codec profile, resolution, bitrate/content,
frame structure, queue depth, and timing endpoints match.

## Benchmark checklist

Record these fields with every result:

- codec, profile, bit depth, chroma format, level, and exact negotiated format;
- visible resolution, coded surface size, pitch, and display target;
- encoded bitrate/bytes, encoder preset, B-frame count, slices, tiles, and WPP;
- decoder resource class, DPB count, and pipeline depth;
- cold-call inclusion/exclusion and sample count;
- gather, submission, ready, drain, presentation, and completed-flip timing;
- frame count, sustained interval, pending maximum, dropped/drained frames;
- output validity, error state, and caller-pool pointer identity; and
- whether the run is controlled synthetic content or practical live content.

The small [timing example](../examples/timing_model.cpp) shows the definitions
without depending on PS5 headers.
