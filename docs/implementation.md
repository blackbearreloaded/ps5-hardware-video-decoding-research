# Minimal real-time streaming implementation guide

## Design rule

Use one decoder/presenter path with a small, explicit mode table. Codec,
profile, level, maximum allocation geometry, accepted coded geometry, pitch,
visible crop, and surface type belong to the mode. Do not build separate H.264
and HEVC subsystems, and do not let an unexpected negotiated format silently
fall through.

The [mode example](../examples/video_modes.cpp) implements this idea without any
PS5 dependency.

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
policies observable in telemetry.

## Per-frame flow

The following is intentionally pseudocode; the exact ABI structures and memory
functions remain platform-specific:

```c
int submit_access_unit(const encoded_access_unit_t *unit)
{
    au_slot = next_au_slot();
    frame_slot = next_frame_slot();

    if (!gather_in_order(unit->fragments, unit->total_bytes, au_slot))
        return DECODER_NEEDS_KEYFRAME;

    input = make_input(au_slot, unit->total_bytes, unit->pts_us);
    frame = make_frame(frame_slot, queried_frame_slot_size);
    output = zeroed_output();

    rc = sceVideodec2Decode(decoder, &input, &frame, &output);
    if (rc != 0)
        return DECODER_NEEDS_KEYFRAME;

    if (!output.valid && pipeline_depth == 1) {
        rc = sceVideodec2Flush(decoder, &frame, &output);
        if (rc != 0)
            return DECODER_NEEDS_KEYFRAME;
    }

    if (!validate_output(selected_mode, &frame, &output, frame_pool))
        return DECODER_NEEDS_KEYFRAME;

    return present_same_pointer(output.buffer, output.buffer_size,
                                output.pitch, output.pitch_bytes,
                                output.height,
                                selected_mode->visible_width,
                                selected_mode->visible_height);
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

In both cases, require one picture, no output error, and exact membership in the
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

## Presentation selection

Keep the established SDR presenter unchanged for H.264 and HEVC Main. Select a
small HDR branch once for a Main10 session:

| Session | Input interpretation | Matrix/transfer | AGC target | VideoOut |
|---|---|---|---|---|
| H.264 / HEVC Main SDR | 8-bit NV12 | Existing Rec.709 SDR conversion | 8:8:8:8 | Existing SDR format |
| HEVC Main10 HDR | Low-aligned 10-bit two-plane | Limited BT.2020 NCL, preserve PQ | 2:10:10:10 | Platform HDR 10-bit format |

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
```

Never label pipelined submission occupancy as hardware decode latency.

## Error and transition policy

Request a new keyframe on decoder errors, unexpected codec/layout, output
pointer violations, or presenter failure. Keep the last valid surface owned
until its GPU/flip work completes.

Treat codec, resolution, bit depth, and HDR state changes as session
reconfiguration boundaries until seamless transitions are independently
proven. Drain decoder and presenter work, release in dependency order, select a
new tuple, and start from an IDR.

## Optimizations worth keeping

1. Exact decoder-to-AGC pointer reuse—already the largest memory-path win.
2. Fixed rotating AU/frame pools and no per-frame allocation.
3. Four H.264 slices at 1080p.
4. HEVC WPP on the host when available.
5. Depth one when live decode fits the frame budget.
6. No synchronous development telemetry in callbacks.
7. Coded/visible geometry separation to avoid copies or rejection of valid
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
- 1440p/4K Main10: not proven by the 1080p controlled result.

## Recommended product rollout

1. Retain H.264 1080p60/four-slice/depth-one as the stable default.
2. Offer HEVC Main at 1080p and 1440p, then 4K as beta until natural-gameplay
   soaks provide repeated latency percentiles.
3. Add 1080p Main10/HDR as an explicit experimental session mode with SDR HUD
   disabled and strict state/layout validation.
4. Validate sustained network HDR10 before enabling it by default.
5. Derive larger Main10 descriptors and native high-resolution scanout only as
   separate measured milestones.
