# DoomMetal

A simple and clean version of the DOOM source code, maintained here as the "DoomMetal" project. This repository contains the classic DOOM engine sources; you still need a valid DOOM IWAD (for example `doom.wad`) to run the game data, Shareware version work fine, or you can buy the full version on Steam.([DOOM STEAM](https://store.steampowered.com/app/2280/DOOM__DOOM_II/)).

This README explains how to build the project on common desktop platforms (Linux, macOS, Windows) using CMake and how to run the resulting executable.

---

## License

See `LICENSE.TXT` in the repository root. This source tree is distributed under the terms listed there (GNU GPL v2 as noted in the original release). You are responsible for owning a valid copy of the DOOM IWAD data files to use with this engine.

---

## Requirements

- Supported platforms: Linux, macOS, Windows (the code uses SDL2 for audio/video and CMake as the build system).
- A C compiler / toolchain (gcc/clang on Unix-like systems, MSVC or MinGW on Windows).
- CMake (recommended minimum 3.21, newer is fine).
- A generator: make, Ninja, or an IDE/Visual Studio on Windows.
- SDL2 development libraries (required for audio/video/input). The package name varies by platform.
- A valid DOOM IWAD (for example `doom.wad`). This project does not include the IWAD — place your copy next to the built executable or provide the path when running.

Platform-specific dependency hints:

- Debian/Ubuntu (Linux):

```bash
sudo apt update
sudo apt install build-essential cmake libsdl2-dev
```

- Fedora (Linux):

```bash
sudo dnf install @development-tools cmake SDL2-devel
```

- Arch (Linux):

```bash
sudo pacman -Syu base-devel cmake sdl2
```

- macOS (Homebrew):

```bash
brew update
brew install cmake sdl2
```

- Windows (two common options):
  - Visual Studio (recommended): install "Desktop development with C++" and CMake support; install SDL2 development files or use vcpkg to install SDL2.
  - MSYS2/MinGW: open MSYS2 MinGW 64-bit shell and install packages:

```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2
```

---

## Build (out-of-source, cross-platform)

This repository uses CMake. We recommend an out-of-source build to keep generated files separate from the source tree. The examples below show common commands for each platform and generator.

General (single-config generators like Make or Ninja):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
```

Note for macOS: `$(nproc)` may not be defined — use `sysctl -n hw.ncpu` or omit the parallel flag.

For multi-config generators (Visual Studio on Windows) specify the configuration when building:

```powershell
# Example for Visual Studio (run in PowerShell or Developer Command Prompt)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

For Ninja (cross-platform):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Notes:
- For a Debug build use `-DCMAKE_BUILD_TYPE=Debug` (or `--config Debug` for multi-config builds).
- If you change critical CMake options, remove the `build` directory (or delete `CMakeCache.txt`) and reconfigure.
- On Windows, if you use MSYS2/MinGW, run the commands from the appropriate MinGW shell so the correct toolchain is picked up.

---

## Running the executable

After a successful build the engine binary will be located in the chosen build directory. On Unix-like systems the binary is typically named `DoomMetal`, on Windows `DoomMetal.exe`.

Examples:

```bash
# Copy your IWAD next to the built binary and run from the build dir (Unix)
cp /path/to/doom.wad build/
cd build
./DoomMetal
```

On Windows (PowerShell / CMD):

```powershell
# From project root (Visual Studio generator example)

#  Copy your IWAD next to the built binary and run from the build dir
build\Release\DoomMetal.exe
```
---