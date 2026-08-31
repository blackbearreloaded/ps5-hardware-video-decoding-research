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

For VP9 carried in WebM or another packet source, inspect the standard
superframe index before submission. The tested decoder rejected a compound
packet containing multiple coded frames, but accepted every frame after the
packet was split. Submit hidden alternate-reference frames even though they
must not be presented. Submit show-existing-frame commands as coded inputs too;
the tested API materialized the referenced picture into the newly supplied
caller-owned frame slot. See the host-side
[packetization example](../examples/vp9/packetization.cpp).

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
work; the HEVC depth-six experiment used six, while the VP9 depth-three
experiment conservatively used six. A later ordered Profile 2 decode/present
control proved that three slots are sufficient for depth three when each
surface is reused only after its completed flip.

### Exact data movement

The proven path has one intentional per-frame CPU payload copy and no decoded-
frame CPU copy:

| Boundary | Operation | Copy? |
|---|---|---|
| Network fragments to AU slot | Validate the complete byte count, then gather fragments in order into one free, persistently mapped direct-memory slot | Yes, CPU |
| AU slot to Videodec2 | Pass the slot address, used byte count, and timestamp in the decoder input | No additional client copy |
| Videodec2 to frame slot | Supply a free caller-owned direct-memory frame slot; the media engine writes decoded planes there | Hardware write |
| Frame slot to AGC | Validate the returned address, derive plane offsets from returned geometry, and bind that exact address as the Y/UV source | No decoded-frame copy |
| AGC to VideoOut target | Sample/convert/scale into the selected framebuffer | GPU render write |

“Zero-copy” therefore describes only the decoder-to-AGC boundary. It does not
mean zero memory traffic, direct socket-to-decoder input, or decoder output used
as the scanout framebuffer. The compressed gather and final GPU render remain.
The exact-pointer proof is also same-process: a raw direct-memory offset from
one process was not a usable mapping handle in another process. Keep the
decoder and presenter in one process unless a separately documented,
authorized inter-process sharing mechanism is available and validated.

The slot ownership sequence is:

```text
AU slot:    free -> receiver/gather -> decoder -> free
Frame slot: free -> decoder -> presenter -> completed-flip fence -> free
```

Never overwrite an AU slot while the decoder may still read it, and never
reuse a frame slot while decoder or GPU/VideoOut work still owns it. The
[host-side memory pipeline example](../examples/common/memory_pipeline.cpp)
models the gather, fixed aligned pools, pointer-membership gate, Y/UV offsets,
same-pointer presentation, and completed-flip release without proprietary
headers.

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
required an immediate `Flush` for every frame. The tested HEVC streams and
controlled depth-one VP9 streams returned valid output directly and required
no immediate flush. At VP9 depth three, the first two calls filled the pipeline,
58 calls returned pictures, and the final two pictures required an
end-of-stream drain. The live rule is:

```cpp
auto output = decoder.decode(access_unit, frame_slot);
if (!output.valid && pipeline_depth == 1) {
    output = decoder.flush(frame_slot);
}
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

The VP9 Profile 0 controls returned format `0` and exact caller-owned frame
addresses. Returned pitch was 2048 bytes at 1920x1080, 2560 at 2560x1440, and
3840 at 3840x2160. A controlled 4K presentation displayed the complete source,
expected color order, diagonal features, and fine checker detail through the
tested linear 8-bit two-plane texture description. Treat this as validation of
that exact Profile 0 path. Profile 2 at 1080p and 4K returned the same pointer
ownership with low-aligned 10-bit words, 1920/3840 and 3840/7680 component/byte
pitches respectively. Its three-slot 4K pipeline also reached completed flips
through the 10-bit target; pixel-level HDR capture was not repeated in that
case.

AGC uses coded height to locate the chroma plane and visible dimensions to crop
or scale. This distinction is required for the proven live 4K layout:

```text
surface: 3840 x 2176 coded, pitch 3840
picture: 3840 x 2160 visible
```

The original decoder controls scaled 1440p and 4K surfaces into a 1920x1080
VideoOut target. Later ProsperoLight integration selected output geometry at
the stream boundary: 1080p uses 1920x1080, 1440p is filtered into 3840x2160,
and 2160p uses a 3840x2160 target 1:1. A true-4K-source oracle then presented
600/600 frames through native 3840x2160 VideoOut at 119.87 FPS on firmware 6.02
and 119.88 FPS on firmware 12.70. See
[High-refresh and native 4K output](high-refresh-output.md) for the measurement
boundary and live-stream acceptance.

The presenter waits until `sceVideoOutGetFlipStatus()` observes its submitted
marker. Reported presentation and callback-to-flip timing therefore includes
AGC work plus display pacing. `sceAgcSuspendPoint()` is a required lifecycle
boundary after GPU submission, and shutdown must drain pending flips/vblank.

The tested AGC command already included the display flip. An early 90-frame
control also issued a second explicit flip and sustained only 29.97 FPS because
each frame waited through two display intervals. Removing only that redundant
flip restored 59.93 FPS. Submit exactly one flip for each presented frame and
use its completion marker as the frame-slot release fence.

## Network and pipeline ownership

Keep socket receive independent of decode and blocking completed-flip
presentation. A deliberately serialized Profile 2 4K control received every
byte and presented every frame, but accumulated 85.917 ms behind a 60 Hz sender
and completed at 55.91 FPS. The access-unit fill itself averaged only 0.266 ms.
Use a bounded producer/consumer queue: the network producer assembles complete
access units, the decoder owns input/frame slots while in flight, and the
presenter releases a decoded slot only after its GPU/flip completion fence.

Bound the queue to the negotiated latency policy and drop/request a keyframe
according to the streaming protocol rather than allowing unbounded buffering.
Record first-byte, AU-ready, output-ready, and completed-flip timestamps on a
common monotonic clock.

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

A separate control completed a background VideoOut flip successfully while the
previous foreground application remained visible. A successful flip proves GPU
and VideoOut completion; it does not grant foreground compositor ownership.
Establish the intended foreground owner before treating a completed flip as
visible presentation evidence.

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

The original crop is evidence-backed. A later controlled full-frame acceptance
copied the complete current affine and enabled destination safe-area state; all
source edges and chart regions were visible. Representative live-content
acceptance remains a separate product test.

The original linear-surface pixel shader used explicit integer raw-buffer
loads: one luma byte and one interleaved UV pair with halved UV coordinates.
That point-sampled path could alias fine detail when reducing 1440p or 4K to a
1080p target. The later product presenter instead hardware-validated filtered
1440p-to-4K sampling while keeping the decoder-to-AGC surface pointer unchanged.
Sampling quality is a renderer property, not an HEVC decode limitation, and
more bitrate cannot repair aliasing introduced after decode.

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

Native 4K scanout avoids reducing a 4K source into a 1080p target and gives a
1440p source one filtered scale into the physical 4K target. It is now proven
at 59.94 and 119.88 Hz, including a true-4K-source 600-frame HFR control on two
firmware versions. This remains a display/presentation result rather than a
decoder optimization; codec capacity must still be measured separately.
