# Codecs and resolutions

## Known-good public game configurations

All proven public configurations use the game-process decoder resource, four
DPB frames, progressive optimization, and a runtime-allocated
compute queue. Pipeline depth is a latency policy; depth one is the production
default.

The table uses permissive client configuration bounds. A bitstream may signal
a lower level than the configured maximum.

| Mode | Profile | Max level | Config maximum | Accepted coded output | Visible picture | Pitch |
|---|---:|---:|---:|---:|---:|---:|
| H.264 1080p | High | 5.1 | 1920x1088 | 1920x1088 | 1920x1080 | 2048 bytes |
| H.264 1440p | High | 5.1 | 2560x1440 | 2560x1440 | 2560x1440 | 2560 bytes |
| H.264 2160p | High | 5.2 | 3840x2176 | 3840x2160 | 3840x2160 | 3840 bytes |
| HEVC Main 1080p | Main | 4.1 | 1920x1088 | 1920x1088 | 1920x1080 | 2048 bytes |
| HEVC Main 1440p | Main | 5.0 | 2560x1440 | 2560x1440 | 2560x1440 | 2560 bytes |
| HEVC Main 2160p | Main | 5.1 | 3840x2176 | 3840x2160 or 2176 | 3840x2160 | 3840 bytes |
| HEVC Main10 1080p | Main10 | 4.1 | 1920x1088 | 1920x1088 | 1920x1080 | 1920 components / 3840 bytes |
| VP9 1080p | Profile 0 | 4.1 | 1920x1080 | 1920x1080 | 1920x1080 | 2048 bytes |
| VP9 1440p | Profile 0 | 5.0 | 2560x1440 | 2560x1440 | 2560x1440 | 2560 bytes |
| VP9 2160p | Profile 0 | 5.1 | 3840x2160 | 3840x2160 | 3840x2160 | 3840 bytes |

The console-validated AVC/HEVC 8-bit modes return NV12. The Main10 mode returns
a two-plane 4:2:0 surface with low-aligned 10-bit values in 16-bit words. The
returned format field did not distinguish the tested 8-bit and 10-bit paths,
so negotiation, byte pitch, dimensions, and buffer size must select the texture
interpretation. VP9 pixel-layout validation remains pending.

## Resolution-specific observations

### 1080p

The hardware's coded surface is 1920x1088 while the visible image is
1920x1080. Eight padded lines belong to the surface layout. For 8-bit modes,
pitch is 2048 bytes, not visible width. For Main10, pitch is 1920 components
and `pitch_bytes` is 3840.

H.264 at four slices is the accepted low-latency product baseline. The live
run averaged 2.268 ms inside Videodec2 plus a 0.096 ms immediate flush.

### 1440p

Both H.264 High and HEVC Main returned 2560x1440 with pitch 2560. Controlled
depth-one decode averaged 9.405 ms for H.264 including flush and 9.872 ms for
HEVC, leaving synchronous room inside a 16.67 ms 60 Hz interval for those
particular streams.

The test presented by scaling into the existing 1920x1080 VideoOut target.
Native 1440p scanout was not part of the result.

### 2160p / 4K

Configure the maximum as 3840x2176. Controlled H.264 and HEVC files returned a
3840x2160 surface, while the practical live HEVC stream returned
3840x2176 coded with 2160 visible lines. All used pitch 3840 and all are valid
observations for their exact codec/workload.

Never make one returned height universal. Keep at least these fields separate:

```text
allocation/config maximum: 3840 x 2176
returned coded surface:    3840 x 2160 or 3840 x 2176
negotiated visible crop:   3840 x 2160
display target:            independently selected (1920 x 1080 in these runs)
```

The practical HEVC stream proved 4K60 decode and scaled presentation. It did
not prove native 4K VideoOut registration, scanout, or Main10 HDR at 4K.

## Codec selection guidance

H.264 is the safest compatibility default and, with four slices at 1080p, has
excellent decoder headroom. HEVC's primary advantage is compression efficiency
at a chosen visual quality or network budget—not guaranteed lower decode time.
In matched byte-rate tests, codec ordering changed with content:

- HEVC was 8.68% faster for the 1080p `testsrc2` pair.
- HEVC was 57.56% slower for a very low-entropy 1080p pair.
- HEVC was 4.97% slower at 1440p and 7.22% slower at 4K.

Use HEVC for higher quality per bit or constrained bandwidth, then benchmark
the actual host encoder and content. Do not advertise both formats as a loose
mask and accept whatever arrives; negotiate the exact selected format and
reject an unexpected codec/layout.

Main10 must be a separate mode requiring the server's exact Main10 capability
bit. Keep HEVC Main available as SDR fallback.

## VP9 and AV1

Platform-interface review found Videodec2 routes for AVC, HEVC, and VP9. A
subsequent firmware-6.02 console control proved the public game resource can
query memory, create/reset the VP9 decoder, accept one locally generated
Profile 0 keyframe, and return one valid, error-free 1920x1080 picture in the
caller-owned direct-memory frame allocation. The returned codec was VP9, with
pitch and byte pitch both 2048.

Matched 60-frame decode-only controls averaged 5.533 ms at 1080p, 8.617 ms at
1440p, and 17.407 ms at 4K after excluding each cold frame. Batch throughput
was 173.09, 112.69, and 55.57 FPS. The single-tile depth-one 4K stream therefore
decoded successfully but did not sustain 60 FPS. Profile 2, deeper pipeline
throughput, alternative tile layouts, pixel-layout validation, and VP9
presentation remain untested.

No equivalent AV1 picture-info API, module route, AOM identifier, or usable
decoder backend was found on firmware 6.02. A protocol layer may be
able to negotiate AV1, but the PS5 decoder backend is missing. Do not advertise
AV1 unless a newer concrete firmware interface is first proven.

Sony's public
[Media Gallery format list](https://www.playstation.com/en-gb/support/hardware/play-video-music-discs-usb-drives/)
is consistent supporting evidence: it lists H.264 and VP9 for USB video up to
3840x2160 and does not list AV1. That consumer application list is not, by
itself, a hardware capability table; the observed platform interfaces are the
stronger local result.

The negative result applies to usable firmware paths. RDNA2 generation alone
does not establish the fixed-function video block retained in Sony's custom
SoC, and it cannot prove physical presence or absence.
