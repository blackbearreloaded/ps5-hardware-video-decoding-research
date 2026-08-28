# Architecture and memory

## Public hardware context

Sony's published base-PS5 specification lists an AMD Radeon RDNA 2-based
graphics engine at variable frequency up to 2.23 GHz / 10.3 TFLOPS, 16 GB of
GDDR6 system memory at 448 GB/s, and display support including 4K 120 Hz. It
does not specify the fixed-function media decoder or derive codec support from
the shader GPU. See the official
[PlayStation 5 hardware specification](https://blog.playstation.com/archive/2020/03/18/unveiling-new-details-of-playstation-5-hardware-technical-specs/).

The single GDDR6 system-memory pool is useful context, but the actionable fact
comes from the console experiments: a direct-memory allocation supplied by the
application was returned by Videodec2 and accepted at the same address by AGC.
That observed API/ownership contract—not bandwidth or TFLOPS alone—proves the
decoder-to-render sharing used by this client.

Do not infer video codecs from “RDNA 2.” AMD's own
[supported-video-format table](https://www.amd.com/en/products/graphics/radeon-for-creators/video-editing.html)
shows neighboring Radeon products with different AV1 columns—for example, RX
6500 without AV1 and RX 6600/6700/6800 with AV1. A custom SoC's firmware and
exposed media backend are the decisive evidence.

## Decoder, GPU, and display roles

Three hardware-facing stages are involved:

| Stage | Responsibility | Output |
|---|---|---|
| Videodec2 | Parse and decode compressed AVC/HEVC/VP9 access units | Caller-owned video surface |
| AGC | Sample the surface, convert Y'CbCr to RGB, scale, and composite | Tiled VideoOut framebuffer |
| VideoOut | Register scanout buffers, pace flips, and drive display output | Completed display flip |

Videodec2 is a hardware media-decoder interface, not a shader implementation.
The AGC render pass is GPU work. Keeping that distinction prevents two common
mistakes: attributing decode time to RDNA compute-unit count, and assuming that
a VideoOut buffer-registration function can make compressed decode faster.

## What zero-copy means here

The application allocates the decoded-frame pool from direct memory. For every
valid output, Videodec2 returns the exact address of one supplied frame slot.
AGC then binds that same address as its Y and UV texture source.

```text
frame slot address supplied to Videodec2
             == Videodec2 output.buffer
             == AGC texture source address
```

This removes a decoded-frame CPU copy, repack, and upload. It does not mean:

- compressed network fragments require no gather;
- the decoded surface is itself a VideoOut scanout buffer;
- AGC performs no read or color-conversion work;
- a completed flip has no vblank wait; or
- every allocation in the process is universally coherent without the API's
  required memory types and cache/lifecycle rules.

The compressed gather is small because it copies encoded bytes, not a full
decoded image. It averaged 2–4 us in accepted H.264 product runs and 5 us in
the practical HEVC 4K runs.

## Access-unit path

The streaming/protocol layer should provide one complete encoded access unit.
Its ordered fragment list and total byte count define the decoder input. For an
HEVC IDR, the list order includes VPS, SPS, PPS, then picture data; P-frames
begin with picture data.

The client should flatten that list once, in order, into a fixed direct-memory
input slot. It should not re-scan for AUD NAL units or reconstruct boundaries
that the protocol layer has already established. The standalone research
parser used AUD NAL type 9 for H.264 and type 35 for HEVC only because it
consumed embedded Annex-B test files rather than pre-framed access units.

The controlled VP9 proof used IVF as an asset container. It skipped the
32-byte IVF file header and the 12-byte per-frame header, then submitted only
the bounds-checked compressed frame payload. A production demuxer should
likewise pass one already-framed VP9 payload rather than IVF or WebM container
bytes.

## Memory contract

Query every size for the selected codec/profile/resolution at runtime. The
proven public game path used the following allocation policy:

| Region | Memory | Purpose |
|---|---|---|
| Compute CPU/GPU workspace | Direct | Videodec2 compute queue |
| Decoder CPU workspace | Flexible | CPU decoder state |
| Decoder GPU workspace | Direct | GPU/media state |
| Optional CPU/GPU workspace | Direct | Shared decoder state |
| Compressed-AU pool | Direct | Contiguous encoded input |
| Decoded-frame pool | Direct | Decoder output and AGC input |

Use the memory protections defined by the authorized platform headers rather
than copying opaque numeric flags from a test build.

Align queried regions and frame slots upward to 16 KiB. A depth-one live
client uses three rotating AU slots and three rotating frame slots to avoid
per-frame allocation and provide safe ownership across decode/present. A
deeper experimental pipeline needs enough distinct slots for all in-flight
work; the depth-six experiment used six.

The frame validation gate must check more than a successful return code:

1. output is valid, error-free, and contains one picture;
2. the frame was accepted when that call returns an output;
3. codec, width, coded height, pitch, and byte pitch match the selected mode;
4. returned byte count fits the allocated slot; and
5. `output.buffer` is an exact member of the caller-owned frame pool.

At pipeline depth greater than one, a successful queue-fill call can return no
output and `accepted == 0`. That is valid; record it as pending and do not
immediately flush. The acceptance requirement applies when a decode call
actually yields an output on the public game path.

## Decode and flush behavior

The tested depth-one H.264 streams returned no picture from `Decode` and
required an immediate `Flush` for every frame. The tested HEVC streams and the
controlled VP9 keyframe returned valid output directly and required no
immediate flush. The live rule is:

```c
decode(access_unit, frame_slot, &output);
if (!output.valid && pipeline_depth == 1)
    flush(frame_slot, &output);
```

Do not blindly flush HEVC, and do not assume that H.264 behavior is universal
across every possible stream. At depth greater than one, drain repeatedly only
at the correct end-of-stream or reconfiguration boundary.

The practical streams had no B frames, so FIFO callback/output correlation was
valid. If B frames are negotiated, correlate output by PTS rather than assuming
decode order equals presentation order.

## Presentation contract

For 8-bit H.264 and HEVC Main, the returned surface is linear NV12: one 8-bit Y
plane followed by interleaved UV, using returned pitch and coded height. For
Main10, each component occupies a 16-bit word but the ten meaningful bits are
low-aligned. See [HDR and Main10](hdr.md).

The VP9 Profile 0 control returned format `0`, a 2048-byte pitch, and the exact
caller-owned frame address for a 1920x1080 picture. Its pixel content and
presentation layout have not yet been independently validated, so do not
promote the AVC/HEVC NV12 interpretation to a VP9 guarantee yet.

AGC uses coded height to locate the chroma plane and visible dimensions to crop
or scale. This distinction is required for the proven live 4K layout:

```text
surface: 3840 x 2176 coded, pitch 3840
picture: 3840 x 2160 visible
```

The current renderer scales 1440p and 4K decoded surfaces into a 1920x1080
VideoOut target. This proves high-resolution decoding and presentation, not
native high-resolution scanout.

The presenter waits until `sceVideoOutGetFlipStatus()` observes its submitted
marker. Reported presentation and callback-to-flip timing therefore includes
AGC work plus display pacing. `sceAgcSuspendPoint()` is a required lifecycle
boundary after GPU submission, and shutdown must drain pending flips/vblank.

## AGC lifetime and display ownership

AGC initialization is process-global rather than per stream. In a repeated
session test, the first session presented normally, while the second decoded
671 access units but presented none because every presentation attempt reached
`sceAgcInit()` again and failed. Retaining the initial AGC state
while releasing and recreating per-stream shaders, command memory, framebuffers,
VideoOut, and decoder resources fixed the failure. Two consecutive sessions in
one process then presented 909/909 and 553/553 frames, with 13.515 ms and
13.502 ms average callback-to-completed-flip latency respectively.

Treat display ownership as a serialized transition. An integration run
occasionally produced black output even though the host stream was healthy,
Videodec2 returned the caller-owned surface, AGC submitted a matching flip
marker, and decoder pending depth stayed zero. The missing video and missing
HUD localized the problem after decode, at the preceding UI renderer-to-AGC
VideoOut handoff. A bounded 100 ms settle after closing the previous renderer
was followed by four visible 60 FPS stream cycles with about 2.29 ms average
decode and 16.7 ms callback-to-flip latency. This supports a handoff race and a
practical mitigation; it does not prove that 100 ms is a universal API rule.

Pre-stream animations or loading frames must obey the same ownership rule.
Stop and join their worker before the first decoder frame uses AGC or a shared
surface, and join it on failed setup before releasing GPU/direct memory.

## Source mapping versus destination viewport

Decoded-surface validation cannot detect a bad texture transform. One inherited
media-renderer affine transform described a partial source viewport and silently
discarded 32 source pixels on the left and 16 on the bottom. The decoder
geometry, pitch, pointer identity, and AGC submission were all valid, so the
symptom initially looked like television overscan.

A preserved source/framebuffer comparison correlated only `0.817160` against
the full source, but `0.996889` after applying that exact crop. Correcting the
source transform restored full-frame mapping in the implementation without
changing decode or zero-copy ownership. Keep these operations independent:

- source coordinates select all visible decoded pixels;
- aspect-fit or aspect-fill maps that source into the destination; and
- a TV-safe inset changes only the destination viewport.

The original crop is evidence-backed. At the recorded milestone, the corrected
mapping had passed build, deployment, and deterministic image checks, while a
matched live edge-chart acceptance run was still pending.

The current linear-surface pixel shader also uses explicit integer raw-buffer
loads. It selects one luma byte and one interleaved UV pair, with UV coordinates
derived by halving the luma coordinates. That is point/nearest sampling, not a
filtered chroma upsample or filtered high-resolution downscale. It is fast and
preserves zero-copy, but it can alias fine detail and colored edges when 1440p
or 4K surfaces are scaled into the 1080p target. This is a rendering-quality
property, not an HEVC decode limitation. A filtered AGC candidate still needs a
matched image-quality and presentation-cost experiment.

## Resource lifecycle

The proven order is:

```text
load the Videodec2 system module
query/allocate compute memory and queue
query/allocate decoder memory
create and reset decoder
allocate persistent AU and frame pools
initialize AGC once for the process, if not already initialized
decode and present
drain AGC work and stop VideoOut
delete decoder
release frame and input pools
release decoder workspaces
release compute queue and memory
unload Videodec2
```

Release display/GPU consumers before freeing the frame pool they reference.
Retain the process-global AGC initialization across reconnects; only its
per-stream consumers belong in the stream teardown sequence.

## Why 4K VideoOut privilege does not reduce decode time

`sceVideoOutAddBuffer4k2kPrivilege` concerns registration of actual 2560x1440
or 3840x2160 scanout buffers. It acts after compressed video has already been
decoded. Decoder submission and output-ready timing end before a scanout buffer
is registered or flipped, so this call cannot speed the codec engine.

Native 4K scanout could avoid scaling into a 1080p target and improve final
image quality or presentation workload. It is a separate display experiment,
not a decoder optimization.
