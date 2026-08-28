# Minimal examples

These examples are deliberately independent of Sony headers, SDK libraries,
firmware, and a PS5 toolchain. They make the observed contracts concrete and
can be compiled on a normal host. Replace the example records with the matching
platform ABI only inside an authorized client build.

All examples use C++20, bounded standard-library types, compile-time contract
checks, and no external dependencies.

## Build and run

With Clang or GCC and C++20:

```sh
mkdir -p build
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/video_modes.cpp -o build/video_modes
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/hdr_contract.cpp -o build/hdr_contract
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror examples/timing_model.cpp -o build/timing_model

./build/video_modes h264 1080
./build/video_modes hevc 2160
./build/video_modes vp9 1080
./build/video_modes main10 1080
./build/hdr_contract
./build/timing_model
```

On Windows, use the same commands from a Clang Developer PowerShell and add
`.exe` to the output names if needed.

## What each example shows

| Use case | Example |
|---|---|
| Select H.264 High at 1080p, 1440p, or 4K | `video_modes.cpp h264 <height>` |
| Select HEVC Main at 1080p, 1440p, or 4K | `video_modes.cpp hevc <height>` |
| Select the proven HEVC Main10 tuple | `video_modes.cpp main10 1080` |
| Select the controlled VP9 Profile 0 tuple | `video_modes.cpp vp9 1080` |
| Preserve maximum, coded, visible, and pitch dimensions | `video_modes.cpp` |
| Calculate the Main10 Y/UV plane layout | `hdr_contract.cpp` |
| Pack the public 10-bit VideoOut word | `hdr_contract.cpp` |
| Apply the preserve-PQ BT.2020-NCL matrix | `hdr_contract.cpp` |
| Distinguish submission, ready, and completed-flip timing | `timing_model.cpp` |

These are contract examples, not a drop-in decoder or a substitute for runtime
memory queries and output validation.
