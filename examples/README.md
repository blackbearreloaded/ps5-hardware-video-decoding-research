# Examples by decoding type

These examples are deliberately independent of Sony headers, SDK libraries,
firmware, and a PS5 toolchain. They make the observed contracts concrete and
can be compiled on a normal host. Replace the example records with the matching
platform ABI only inside an authorized client build.

All examples use C++20, bounded standard-library types, compile-time contract
checks, and no external dependencies.

## Layout

| Area | Examples |
|---|---|
| [H.264](h264/) | Proven High-profile mode and surface contracts |
| [HEVC](hevc/) | Proven Main and Main10 mode and surface contracts |
| [VP9](vp9/) | Profile 0/2 modes, superframe splitting, and presentation policy |
| [HDR](hdr/) | 10-bit plane layout, BT.2020/PQ conversion, and output packing |
| [Common](common/) | Shared mode validation and end-to-end timing boundaries |

## Build and run

With Clang or GCC and C++20:

```sh
mkdir -p build
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/h264/modes.cpp -o build/h264_modes
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/hevc/modes.cpp -o build/hevc_modes
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/vp9/modes.cpp -o build/vp9_modes
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/vp9/packetization.cpp -o build/vp9_packetization
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/hdr/surface_contract.cpp -o build/hdr_surface_contract
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/common/timing_model.cpp -o build/timing_model

./build/h264_modes high 1080
./build/hevc_modes main 2160
./build/hevc_modes main10 1080
./build/vp9_modes profile0 1080
./build/vp9_modes profile2 2160
./build/hdr_surface_contract
./build/timing_model
./build/vp9_packetization
```

On Windows, use the same commands from a Clang Developer PowerShell and add
`.exe` to the output names if needed.

## What each example shows

| Use case | Example |
|---|---|
| Select H.264 High at 1080p, 1440p, or 4K | [`h264/modes.cpp`](h264/modes.cpp) |
| Select HEVC Main at 1080p, 1440p, or 4K | [`hevc/modes.cpp`](hevc/modes.cpp) |
| Select the proven HEVC Main10 tuple | [`hevc/modes.cpp`](hevc/modes.cpp) |
| Select VP9 Profile 0 at 1080p, 1440p, or 4K | [`vp9/modes.cpp`](vp9/modes.cpp) |
| Select VP9 Profile 2 at 1080p or 4K | [`vp9/modes.cpp`](vp9/modes.cpp) |
| Split VP9 superframes and suppress hidden-frame presentation | [`vp9/packetization.cpp`](vp9/packetization.cpp) |
| Calculate HEVC Main10 and VP9 Profile 2 Y/UV plane layouts | [`hdr/surface_contract.cpp`](hdr/surface_contract.cpp) |
| Pack the public 10-bit output word and apply the BT.2020-NCL matrix | [`hdr/surface_contract.cpp`](hdr/surface_contract.cpp) |
| Distinguish submission, ready, and completed-flip timing | [`common/timing_model.cpp`](common/timing_model.cpp) |
| Reuse mode lookup, parsing, and output validation | [`common/video_mode.hpp`](common/video_mode.hpp) |

These are contract examples, not a drop-in decoder or a substitute for runtime
memory queries and output validation.
