# Pixie

A pixel art image converter. It loads an image in any common format, and
remaps every pixel to the closest color in a chosen color palette using the
CIEDE2000 perceptual color-difference formula.

- Rendering / windowing: SDL3
- UI: Dear ImGui (SDL3 + SDL_Renderer backend)
- Image IO: stb_image / stb_image_write (reads virtually any format, writes PNG)
- Palettes: GIMP `.gpl` files
- File dialogs: Native File Dialog Extended (native OS open/save dialogs)

## Prerequisites

- **Visual Studio 2022** with the "Desktop development with C++" workload
  (make sure "C++ CMake tools for Windows" is checked).
- **[vcpkg](https://github.com/microsoft/vcpkg)**, with the `VCPKG_ROOT`
  environment variable pointing at your vcpkg checkout. If you don't have it:

  ```powershell
  git clone https://github.com/microsoft/vcpkg
  .\vcpkg\bootstrap-vcpkg.bat
  setx VCPKG_ROOT "C:\path\to\vcpkg"
  ```

  (Restart your shell / Visual Studio after `setx` so it picks up the new
  environment variable.)

All C++ dependencies (SDL3, Dear ImGui, stb, nativefiledialog-extended) are
declared in [`vcpkg.json`](vcpkg.json) and are fetched/built automatically by
vcpkg the first time you configure the project — no manual downloads needed.

## Building

### Visual Studio (recommended)

1. `File -> Open -> Folder...` and select this directory.
2. Visual Studio detects `CMakePresets.json` and configures automatically
   (this first run builds SDL3, Dear ImGui, etc. via vcpkg, so it can take a
   few minutes).
3. Select the `Pixie.exe` startup item and press F5 / Ctrl+F5.

### Command line

```powershell
cmake --preset windows
cmake --build --preset windows-release
.\build\windows\Release\Pixie.exe
```

A `Pixie.sln` you can double-click can be generated instead of using Ninja,
if you'd rather have a classic solution file:

```powershell
cmake -B build/vs -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

## Usage

1. **Open Image...** — load the source image (PNG, JPEG, BMP, TGA, GIF, PSD, HDR, ...).
2. **Open Palette (.gpl)...** — load a GIMP palette file. A sample palette is
   included at `assets/palettes/pico-8.gpl`.
3. **Convert** — remaps every source pixel to its closest palette color
   (CIEDE2000 distance in CIE L\*a\*b\* space), using all available CPU cores.
4. **Save Result As PNG...** — write the converted image out.

## Project layout

```
src/
  core/        Palette (.gpl) parsing, image IO, sRGB<->Lab + CIEDE2000, converter
  app/         SDL3 window/renderer, Dear ImGui UI, texture upload helpers
  main.cpp
assets/palettes/  Sample .gpl palettes
```
