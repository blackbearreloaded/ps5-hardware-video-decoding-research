# Open investigation: live 4K120 HDR stuttering

Status at publication, September 2026: unresolved. Native 4K high-refresh output
is established separately; neither that result nor successful HDR rendering
guarantees sustained natural-gameplay decoding and presentation at 120 FPS.

## Research question

Which stage first exceeds its timing or buffering budget when bitrate or
content complexity increases in a live Main10 stream? Separate decoder work
from host capture/encoding, transport, receiver scheduling and presentation.
Application support reports and release comparisons are outside this document's
scope; no particular host or client implementation is assigned a root cause.

## What increasing bitrate changes

At fixed resolution and FPS, bitrate controls the compressed data budget and
quality tradeoff, not additional pixels or additional frames. Actual rates vary
with encoder policy and content. Higher rates can increase packet processing,
frame reassembly and decoder work; bursts can matter more than the average.
Codec complexity is not determined by byte count alone.

There is no demonstrated universal PS5 bitrate ceiling. One measurement
reported an effective receive buffer smaller than requested; that observation
alone does not establish packet loss, a throughput ceiling, or the root cause.
Wired transport removes some variability but does not eliminate queueing or
scheduling stalls. Averages near 8 ms leave little apparent margin relative to
the 8.33 ms frame interval, but different pipeline stages can overlap: do not
sum unlike latency counters or infer a throughput limit from those averages.

## Evidence needed next

Use the [bounded probe protocol](performance-probes.md). Keep host and client
configuration fixed while capturing the failing interval.

| Observation during a stall | Next boundary to investigate, not a verdict |
|---|---|
| Decode calls exceed the frame interval | Main10/content complexity, verified slice layout, decoder scheduling |
| First-receive or reassembly gaps grow | Host capture/encode, transport bursts, receive-worker scheduling |
| Enqueue-to-callback or pending frames grow | Consumer scheduling and decode/presentation backpressure |
| Completion waits grow while decode stays short | GPU work, source ownership and presentation cadence |
| Host-reported processing spikes | Host-side capture/encoding, confirmed with host logs |

Once a failing gameplay window is captured, change only one relevant variable
(for example 80 to 40 Mbps, or HDR to SDR). Recreate the stream with that
configuration, use the same scene, and inspect percentiles plus stall timing.
Separate requested FPS, host display refresh, actual frame arrival, decoded
frames and completed presentation. Zero recorded frame gaps does not mean
stable cadence. No root cause, fix, guaranteed rate, or universal slice policy
is claimed by this document.
