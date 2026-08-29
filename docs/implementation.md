# Minimal real-time streaming implementation guide

## Design rule

Use one decoder/presenter path with a small, explicit mode table. Codec,
profile, level, maximum allocation geometry, accepted coded geometry, pitch,
visible crop, and surface type belong to the mode. Do not build separate H.264
and HEVC subsystems, and do not let an unexpected negotiated format silently
fall through.

The codec-specific [H.264](../examples/h264/modes.cpp),
[HEVC](../examples/hevc/modes.cpp), and [VP9](../examples/vp9/modes.cpp) mode
examples implement this idea without any PS5 dependency.

## Session setup

1. Read the user's exact codec, resolution, and HDR selection.
2. Confirm the peer capability for that exact codec profile and bit depth.
3. For HDR, require Main10, Rec.2020, limited range, active HDR state, and the
   title capability documented in [HDR and Main10](hdr.md).
4. Load the Videodec2 system module through the authorized platform interface.
5. Query and allocate compute memory, then create the default compute pipe/queue.
6. Query decoder memory for the selected tuple; allocate every returned region
   and frame size at runtime with 16 KiB alignment.
7. Create/reset the game-process decoder resource with four DPB frames and pipeline
   depth one.
8. Allocate fixed AU and caller-owned frame pools; do not allocate per frame.
9. Initialize AGC once per process if necessary, then create one SDR-NV12 or
   Main10-HDR presenter branch for the session.

Use H.264 four-slice capability at 1080p. For HEVC, ask the host encoder for
WPP where it exposes a suitable control, then verify the PPS flag. Keep both
policies observable in telemetry. For VP9 4K, request four tile columns as the
measured starting policy and verify the bitstream rather than trusting an
encoder option name.

Run network receive and access-unit assembly as a bounded producer independent
of decode and completed-flip presentation. The measured serialized path lost no
frames but accumulated receive backlog. Backpressure must be explicit; an
unbounded jitter queue merely converts overload into latency.

## Per-frame flow

The following is intentionally pseudocode; the exact ABI structures and memory
functions remain platform-specific:

```cpp
SubmitResult submit_access_unit(const EncodedAccessUnit& unit)
{
    auto& au_slot = input_pool.acquire();
    auto& frame_slot = frame_pool.acquire();
    if (!gather_in_order(unit.fragments, unit.total_bytes, au_slot)) {
        return SubmitResult::needs_keyframe;
    }

    auto input = make_input(au_slot, unit.total_bytes, unit.pts);
    pending.push(unit.frame_flags, frame_slot);
    auto output = decoder.decode(input, frame_slot);
    if (!output && pipeline_depth == 1) {
        output = decoder.flush(frame_slot);
    }
    if (!output) {
        return SubmitResult::queued;
    }
    if (!validate_output(selected_mode, *output, frame_pool)) {
        return SubmitResult::needs_keyframe;
    }

    const auto completed = pending.pop_front();
    if (completed.show_frame || completed.show_existing_frame) {
        presenter.present_same_pointer(*output, selected_mode.visible_size);
    }
    return SubmitResult::accepted;
}
```

Preserve the presentation timestamp for decoder input and telemetry. If output
order can differ from input order, correlate by PTS.

## Validation by surface type

For 8-bit NV12:

- require the selected AVC or HEVC Main codec;
- require exact output width and pitch;
- accept only the proven coded heights for that resolution;
- compute UV base using returned pitch and coded height; and
- crop/draw using visible dimensions.

For 1080p Main10:

- require the negotiated HEVC Main10 mode, width 1920,
  coded height 1088, logical pitch 1920, and byte pitch 3840;
- require returned bytes to fit the queried/aligned slot;
- treat 16-bit components as low-aligned 10-bit; and
- bind the exact returned pointer to the session's HDR descriptors.

For VP9 Profile 2:

- require the exact negotiated profile rather than inferring it from the
  returned format field;
- at 1080p require 1920 components / 3840 bytes of pitch;
- at 4K require 3840 components / 7680 bytes of pitch;
- interpret active words as low-aligned 10-bit two-plane 4:2:0; and
- keep color space, range, and transfer as independent stream state.

In every case, require one picture, no output error, and exact membership in the
caller frame pool. A valid-looking pointer outside that pool breaks the proven
ownership contract.

## Queue-depth policy

Depth one is the low-latency default and sustained the practical live 4K60
test. Consider depth three only when repeated live depth-one p95/p99 decode
time approaches the 16.67 ms frame period.

If depth is greater than one:

- permit queue-fill submissions with no output and `accepted == 0`;
- track every pending input timestamp;
- never reuse a slot still owned by an in-flight frame;
- do not immediate-flush every no-output call;
- drain repeatedly at end of stream; and
- expect added residency even when submission calls become very short.

Do not use depth as a cosmetic “performance mode.” Expose it only with actual
ready-latency and throughput telemetry.

## VP9 packet and display policy

Split a VP9 compound superframe using its standard trailing index before
submitting coded frames. The measured decoder rejected the compound packet as
one access unit but accepted every split coded frame. Carry parsed `show_frame`
and `show_existing_frame` state beside each queued timestamp:

- submit hidden alternate-reference frames and suppress their presentation;
- submit show-existing-frame commands and present the returned materialized
  caller-owned output; and
- correlate output to queued coded frames, not container packets.

The [C++20 packetization example](../examples/vp9/packetization.cpp) implements
the superframe-index split and display decision without platform dependencies.

## Presentation selection

Keep the established SDR presenter unchanged for H.264 and HEVC Main. Select a
small HDR branch once for a Main10 session:

| Session | Input interpretation | Matrix/transfer | AGC target | VideoOut |
|---|---|---|---|---|
| H.264 / HEVC Main SDR | 8-bit NV12 | Existing Rec.709 SDR conversion | 8:8:8:8 | Existing SDR format |
| VP9 Profile 0 SDR | 8-bit two-plane | Selected stream matrix/transfer | 8:8:8:8 | Existing SDR format |
| HEVC Main10 or VP9 Profile 2 HDR | Low-aligned 10-bit two-plane | Limited BT.2020 NCL, preserve PQ when signaled | 2:10:10:10 | Platform HDR 10-bit format |

Recreate VideoOut only at a safe owner/session boundary. A protocol-level HDR
control callback should record state, not destroy graphics resources from its
callback thread.

## Presenter ownership and mapping

Only one renderer should own VideoOut/AGC presentation during a transition.
Fully stop a launcher or loading renderer before opening the stream presenter,
and join any animation worker before the first decoded frame or failed-setup
teardown. A bounded 100 ms post-launcher settle removed an observed intermittent
black handoff across four hardware cycles, but it is an integration mitigation,
not a documented timing guarantee.

Retain only process-global AGC initialization across sessions. Shaders, command
memory, VideoOut buffers, and decoder/frame pools remain per-session resources
and must still drain and release in dependency order.

Keep source and destination transforms separate. Texture coordinates must cover
the complete visible decoded rectangle. Aspect fitting and TV-safe margins
belong to the destination viewport; applying an inherited partial-view affine
matrix to the source can crop valid pixels while every decoder and zero-copy
check continues to pass.

The proven raw-buffer shader uses integer point samples. Treat filtered chroma
upsampling and 1440p/4K-to-1080p downscaling as a separate renderer experiment;
compare a fixed edge/text chart and record presentation cost before changing a
default. More bitrate cannot correct sampling aliasing introduced after decode.

## Telemetry

Capture cheap timestamps/counters in the hot path and publish aggregates
outside it. Opening sockets or formatting large reports inside the decode or
present callback consumed receive headroom in early tests.

At minimum expose:

```text
codec/profile/bit depth, visible and coded dimensions, pitch
slice count or HEVC WPP, resource class, pipeline depth
compressed gather average/max
decode submission average/max
submission-to-ready average/min/max
present average/max
callback-to-completed-flip average/min/max
pending maximum, presented FPS, drops/drains/errors
decoder-to-pool-to-AGC pointer-identity result (diagnostic builds)
network first-byte, AU-ready, queue residency, and receive backlog
```

Never label pipelined submission occupancy as hardware decode latency.

## Error and transition policy

Request a new keyframe on decoder errors, unexpected codec/layout, output
pointer violations, or presenter failure. Keep the last valid surface owned
until its GPU/flip work completes.

Malformed VP9 input produced a decoder error, after which reset recovered on a
known-good keyframe without process restart. Use that bounded recovery path:
stop consuming dependent frames, reset, request a keyframe, and resume only
after strict output validation succeeds.

Keyframe-driven VP9 Profile 0 changes from 1080p to 4K and back succeeded in one
decoder configured for the 4K maximum. Treat codec, profile, bit depth, and HDR
changes as session reconfiguration boundaries. Resolution changes within a
proven maximum may use a guarded fast path, but must update returned geometry
and presenter descriptors before display.

## Optimizations worth keeping

1. Exact decoder-to-AGC pointer reuse—already the largest memory-path win.
2. Fixed rotating AU/frame pools and no per-frame allocation.
3. Four H.264 slices at 1080p.
4. HEVC WPP on the host when available.
5. Four VP9 tile columns for the tested 4K policy, remeasured per encoder.
6. Separate bounded network receive and decode/present ownership.
7. Depth one when live decode fits the frame budget.
8. No synchronous development telemetry in callbacks.
9. Coded/visible geometry separation to avoid copies or rejection of valid
   aligned surfaces.

## Ideas rejected or deferred

- Non-public media-process control: faster synchronous synthetic depth-one
  decode, but no deeper-queue throughput scaling. Its private setup details are
  intentionally not integration guidance.
- Unconditional deep pipelines: more throughput, substantially more latency.
- Eight H.264 slices at 1080p: tiny extra decode gain, about 9% more network
  work, no completed-flip improvement.
- CPU repack/upload: defeats the caller-owned direct-surface contract.
- `sceVideoOutAddBuffer4k2kPrivilege` as a decoder optimization: display-buffer
  registration occurs after decode.
- AV1 negotiation: no firmware-6.02 decoder backend.
- 1440p HEVC Main10: not proven by the 1080p controlled result.
- Alternate decoder resource classes: memory query alone did not make decoder
  creation available, so no performance comparison exists.

## Recommended product rollout

1. Retain H.264 1080p60/four-slice/depth-one as the stable default.
2. Offer HEVC Main at 1080p and 1440p, then 4K as beta until natural-gameplay
   soaks provide repeated latency percentiles.
3. Add 1080p HEVC Main10/HDR as an explicit experimental session mode with SDR HUD
   disabled and strict state/layout validation.
4. Add VP9 Profile 0 behind strict superframe/show-frame handling; use four
   tiles as a measured 4K starting point.
5. Treat VP9 Profile 2 4K as experimental until representative live content
   and display-side HDR correctness are accepted.
6. Keep native 4K VideoOut selection separate from decoder policy.
