# 10-bit decoded surfaces and HDR10

## Proven HEVC Main10 1080p end-to-end contract

A normal game-category process can decode a controlled HEVC Main10 BT.2020/PQ
frame into caller-owned direct memory, bind that exact pointer in AGC, convert
limited-range BT.2020 non-constant-luminance Y'CbCr to PQ-coded RGB, render to
a 10-bit target, and flip through public HDR VideoOut.

| Layer | Proven value |
|---|---|
| Application category | Normal game process |
| Application metadata | HDR-capable title metadata is required |
| Decoder resource | Game-process decoder resource |
| Codec/profile/level | HEVC Main10, Level 4.1 |
| Maximum / visible | 1920x1088 coded / 1920x1080 visible |
| Returned surface | Caller-owned two-plane 4:2:0, low-aligned 10-bit words |
| Pitch | 1920 components / 3840 bytes |
| Returned allocation | Runtime-queried and aligned to 16 KiB |
| AGC target | 2:10:10:10 UNORM color target |
| VideoOut format | Platform HDR 10-bit output format |
| Packed output word | A2B10G10R10; R in bits 9:0, G 19:10, B 29:20, A 31:30 |
| Transfer | Preserve PQ; do not decode/re-encode or apply PQ twice |

Changing only the VideoOut format was insufficient: buffer registration failed.
The required HDR application metadata enabled the public path. Changing process
category alone did not grant HDR, so a normal HDR-capable game process is the
preferred low-latency baseline. Use authorized platform tooling to produce that
metadata rather than copying opaque values from a test package.

## Main10 decoded surface

The surface has one 16-bit word per component, but it is not conventional
MSB-aligned P010. The ten sample bits are in bits 9:0. Controlled black luma
samples used the expected limited-range value 64; a full allocation scan found
frequent use of bits 5:0 and essentially no image samples in bits 15:10.

Use returned component pitch and coded height:

```cpp
const auto y_bytes = pitch_components * coded_height * std::size_t{2};
const auto uv_base = base + y_bytes;
const auto total_raw = pitch_components * coded_height * std::size_t{3};
const auto slot_size = align_up(total_raw, std::size_t{16} * 1024);
```

For 1920x1088 this calculation, rounded to 16 KiB, exactly matched the decoder's
runtime-queried frame allocation. The returned format field alone did not
distinguish bit depth.

## VP9 Profile 2 contract

VP9 Profile 2 uses the same observed low-aligned 10-bit two-plane storage
family. The tested 1080p surface used 1920 component pitch / 3840 byte pitch;
the tested 4K surface used 3840 / 7680. Both were exact members of the
application's caller-owned frame pool.

A four-tile 4K Profile 2 stream decoded at 72.37 FPS at depth one and 170.51
FPS at depth three. The three-slot depth-three pipeline then presented all 60
decoded surfaces through the 10-bit HDR target at 59.53 FPS paced cadence.
Every output reached a completed flip, but the automated remote capture did not
provide new pixel-level evidence for that 4K run. Treat storage, pointer
identity, and completed presentation as proven; retain the earlier controlled
HDR chart as the color-path validation.

Profile 2 is not itself proof that content is HDR. Select BT.2020/PQ rendering
only when the stream independently signals the required color primaries,
matrix, range, and transfer function.

## Color conversion

HDR10 HEVC samples are already nonlinear PQ-coded Y'CbCr. For BT.2020
non-constant-luminance input, apply the matrix to normalized primed values and
leave the result PQ-coded:

```text
R' = Y' + 1.474600 Cr'
G' = Y' - 0.164553 Cb' - 0.571353 Cr'
B' = Y' + 1.881400 Cb'
```

Build the matrix and limited-range normalization through the authorized graphics
API from these semantic coefficients. Opaque command-buffer or register words
are deliberately outside this publication.

Do not apply an SDR gamma transform, a linear-light Y'CbCr matrix, or another
PQ OETF. Any of those changes would alter the transfer function a second time.

## AGC and VideoOut output

Select the platform's 2:10:10:10 AGC color target instead of its SDR 8:8:8:8
target, while retaining the proven UNORM/clamp state. Pair it with the matching
HDR 10-bit VideoOut format from authorized platform headers.

The public output format consumes packed words as:

```text
31          30 29                 20 19                 10 9           0
+-------------+---------------------+---------------------+-------------+
|  A (2 bits) |      B (10 bits)    |      G (10 bits)    | R (10 bits) |
+-------------+---------------------+---------------------+-------------+
```

The [HDR contract example](../examples/hdr/surface_contract.cpp) validates that packing
and the Main10 plane calculation on a normal desktop compiler.

Create the Y and UV texture descriptors from the runtime addresses, component
format, pitch, and coded dimensions through the authorized graphics interface.
Do not reuse precomputed opaque descriptor words. The validated geometry in
this study is limited to the 1920-wide view; derive and validate descriptors
independently before using larger Main10 modes.

## Stream negotiation

The protocol layer must expose the required state:

```text
codec       = HEVC Main10
color space = BT.2020
color range = limited
HDR state   = active
```

Require the server's exact Main10 capability rather than a broad HEVC flag.
Capture HDR state changes, and validate each access unit's HDR and color-space
metadata before choosing the HDR renderer. Keep HEVC Main/Rec.709 as an
explicit SDR fallback.

Main10 availability and active HDR are independent signals. In one integrated
session, the peer advertised exact Main10 support, negotiated Main10, and
allowed the Videodec2 instance to create/reset successfully, while
the live HDR callback and every decode unit still reported HDR inactive. The
requested color space remained Rec.2020. A strict guard rejected the units
before compressed-byte gather or decode, proving that codec negotiation alone
must not switch VideoOut into HDR mode.

When this mismatch occurs, report it once and end or renegotiate the session.
Do not remain in an unbounded reject loop that presents a black screen.

For a first implementation, make HDR a session-level choice. If the host
changes HDR state, stop or drain queued work, recreate the correct presenter at
a safe owner boundary, and request/wait for an IDR. Seamless mid-stream
transitions have not been hardware-tested.

Retain stream mastering metadata for telemetry and future policy. The system's
tone-map luminance query reports display-side luminance information; it does
not replace the stream's mastering primaries, MaxCLL, or MaxFALL.

## UI and screenshot cautions

An SDR HUD cannot be composited into a PQ target using unchanged 8-bit white
values. Initially disable the HUD in HDR sessions or deliberately map UI colors
to chosen PQ luminances.

A later source-level integration candidate kept Main10 video on the validated
shader, switched to a separate SDR pixel shader for a second HUD draw in the
same AGC command buffer, and limited an 8-bit neutral-NV12 text surface to a
paper-white-range luma before constant-alpha blending. It passed build and
launcher integration, but no live HDR stream acceptance was completed. Treat
this as a bounded implementation direction, not a console-proven HDR HUD.

Remote Play tone maps/clips HDR captures. It proved channel order and gross
band ordering, but it is not a photometer and does not prove HDMI mastering
metadata, peak luminance, or gamut accuracy. The AGC output target is also
tiled/swizzled; a CPU-linear `y * width + x` address is not a valid pixel
readback coordinate.

The native 3840x2160/119.88 Hz result is an SDR fixed-source presentation proof
and a live H.264 product acceptance. It does not by itself prove Main10/HDR at
4K/120. HDR changes the decoded surface interpretation, shader conversion,
framebuffer format, VideoOut state, and display metadata; repeat the complete
measurement rather than inheriting the SDR result.

## What remains to prove

- sustained 1080p60 network Main10/HDR with natural content;
- decode and end-to-end latency percentiles under that workload;
- host HDR on/off transitions and SDR fallback;
- live HDR-qualified UI/HUD composition;
- 1440p and 4K HEVC Main10 decoder tuples and texture descriptors;
- representative live VP9 Profile 2 HDR content and latency percentiles;
- native 1440p/4K Main10/HDR-to-scanout integration, including 119.88 Hz; and
- display-side photometric and metadata verification.

Do not expose larger Main10 modes by extrapolating the 1080p result.
