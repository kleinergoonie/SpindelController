SpindelController RP2040 Zero

This clone is prepared for targeting the Waveshare RP2040 Zero board.

Quick build instructions (Windows):

1. Install Raspberry Pi Pico SDK and toolchain as used in the original project.
2. Open PowerShell and create a build dir:

```powershell
mkdir build
cd build
cmake -G Ninja -DPICO_BOARD=waveshare_rp2040_zero -DCMAKE_BUILD_TYPE=Release ..
ninja
```

3. The UF2 will be located in the `build` folder after `pico_add_extra_outputs` runs.

Note: This clone includes a minimal `cmake/board_rp2040_zero.cmake` file with pin defaults. Adjust pin assignments in `CMakeLists.txt` or this file as needed.
