# Bounded live-stream performance probes

This describes a September 2026 development probe. Summary klog
delivery was accepted on console; the new five-second window aggregation passed
host tests and was deployed, but its representative gameplay HDR run was still pending
when this document was written. It is instrumentation, not a performance fix.

## Collection contract

Use preallocated frame records written by the decoder/presenter owner. Do not
allocate, format JSON, or write to klog from each streaming callback. After
workers stop, serialize the session summary and aggregate the retained trace.
Preserve the local summary as a fallback if kernel logging fails.

The probe retains at most 32,768 frames (about 273 seconds at 120 FPS) and stops
recording when full. It emits at most 300 occupied five-second windows. This
keeps memory and post-stream output bounded; it is not continuous monitoring.

An application-neutral example envelope is:

```text
[video-perf] session=N part=P/T json=...
```

Payload chunks contain at most 384 bytes, with a complete log line below 512
bytes. Concatenate payloads in part order, excluding the envelope and trailing
log newline. Each new JSON record restarts at part one; window records share
the session number of their summary. Session numbers restart with the process.
Require all parts, reject interrupted/interleaved sequences, and validate JSON;
do not silently combine separate records or separate app launches.

The first object is the session summary. Subsequent objects use `kind=window`;
the final object uses `kind=windows_end` with `recorded`, `reported`, and
`omitted` counts. No footer, missing parts, or unequal recorded/reported counts
mean coverage is incomplete. A nonzero omitted count means the trace filled.
Capture before connecting and continue through stream teardown and the footer.

## Measurement boundaries

| Fields | Meaning / limitation |
|---|---|
| `start_s`, `duration_s` | Five-second callback-time bin relative to first callback; last bin may be partial, empty bins omitted |
| `frames`, `presented`, `bytes`, `gaps` | Callback records, completed-flip observations associated with those records, compressed bytes, frame-number gaps |
| `decode_mean_us`, `decode_p99_upper_us`, `decode_max_us` | Synchronous decoder-call time; p99 is a histogram upper bound, not the exact CSV percentile |
| `decode_over_budget`, `budget_us` | Calls above the rounded-up requested-FPS interval (8,334 us at 120 FPS) |
| `host_count`, `host_mean_us`, `host_max_us` | Host-reported processing duration; zero/missing samples are not evidence of zero host cost |
| `receive_gap_max_us` | Gap between first-receive timestamps of successive callback records; can span windows |
| `reassembly_max_us` | First receive to enqueue, using ordered timestamps from the same clock |
| `queue_max_us`, `pending_max` | Enqueue-to-callback maximum and sampled pending video frames |
| `flip_gap_max_us`, `prior_flip_wait_max_us` | CPU-observed completion interval and explicit wait for a prior flip |

Do not subtract absolute host and client timestamps. These are CPU observations,
not GPU timestamps or input-to-photon measurements. A submitted frame is not
necessarily completed. Window counts are attributed by callback time, so they
are not exact physical scanout counts within that wall-clock interval.

Do not divide a partial bin's frame count by five and call it sustained FPS.
Correlate high decode times, receive gaps, queue pressure and presentation waits;
none individually proves the source of a stall. Zero frame-number gaps do not
prove zero packet loss, since recovery may conceal loss.

## Reproducible gameplay protocol

1. Record build flags, codec/profile/HDR, resolution, requested and actual output
   refresh, bitrate, audio mode, network type, firmware and host software.
2. Complete Windows/RDP login in a separate stream. Start the game fresh with
   the desired configuration; distinguish reconnect/menu time from gameplay.
3. Warm up 30 seconds, then play the same scene for two minutes. Note approximate
   times of visible stalls. Stop early if the stream is unusable.
4. Return to the launcher and retain klog through the final summary/footer.
5. Analyze the gameplay session on its own. Preserve cold and transition data
   but exclude it explicitly from steady-state claims.

An example HDR investigation uses a repeatable scene, HEVC Main10, 4K120,
a fixed bitrate, and Ethernet. Do not change host encoder settings during that run. A later
single-variable bitrate test can distinguish workload sensitivity without
requiring a comparison against an older application release.

Share only sanitized metrics: no host identity, typed input, pairing material,
images, or entire save containers. This research repo intentionally omits raw
laboratory captures and proprietary artifacts. See [publication policy](../PUBLICATION.md).
