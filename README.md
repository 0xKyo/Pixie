# Pixie

A pixel art image converter. Load an image in any common format, optionally
remove its background, crop it, shrink it down to a pixel-art resolution, and
remap every pixel to the closest color in a chosen palette — all via the
CIEDE2000 perceptual color-difference formula.

- Rendering / windowing: SDL3
- UI: Dear ImGui (SDL3 + SDL_Renderer backend)
- Image IO: stb_image / stb_image_write (reads virtually any format, writes PNG)
- Palettes: GIMP `.gpl` files
- File dialogs: Native File Dialog Extended (native OS open/save dialogs)

## Features

- **Palette conversion** — every pixel is remapped to its closest color in
  the loaded palette, measured with CIEDE2000 in CIE L\*a\*b\* space. If no
  palette is loaded, Convert still runs the other steps below and passes
  colors through unchanged.
- **Remove Background** — flood-fills the background inward from the image
  border (CIEDE2000 within a tolerance), forcing it transparent. Click the
  source preview to include/exclude a region (magic wand).
- **Crop** — auto-detects a tight bounding box around the subject (reusing
  the same background detection), with draggable handles on the source
  preview to adjust it by hand.
- **Downscale** — shrinks the image to a pixel-art width (16/32/64/128px,
  height keeps the aspect ratio) using coverage-weighted box-filter
  averaging, which keeps silhouettes legible without the noise nearest-
  neighbor sampling would introduce. A checkbox controls whether the palette
  swap happens before or after the shrink, so both orders can be compared.

## Prerequisites

- **Visual Studio 2022 or newer** with the "Desktop development with C++"
  workload (make sure "C++ CMake tools for Windows" is checked).
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

Pixie opens on an **Asset Viewer**. The first time, it asks for your assets
folder (remembered afterwards; change it with **Change Folder...**). It scans
the folder recursively and shows a thumbnail grid of every `.png`, `.jpg` and
`.aseprite`. A base image and its conversion are paired by folder + name
(`hero.png` <-> `hero.aseprite`) and shown as one asset; a green **ASE** chip
means it already has a converted version.

- **Click** an asset to open it in the converter (base on the left, existing
  `.aseprite` on the right). **Save Beside Original** writes
  `<name>.aseprite` next to the PNG/JPG, asking before overwriting.
- **Ctrl+click** toggles single assets and **Shift+click** selects the range from
  the last clicked one (like Explorer). Then **Convert Selected** to apply the same
  tools to all and write each `.aseprite` beside its original.
- Search, and filter by Converted / Not converted, from the top bar.

### Converter

1. **Open Image(s)...** — load one or more source images (PNG, JPEG, BMP, TGA, GIF, PSD, HDR, ...).
   With several selected, they're listed in the sidebar; click one to preview
   and tune the tools on it. **Convert All (N) To Folder...** then applies the
   same settings to every image and writes `<name>_01.png`, `<name>_02.png`, ... into the folder
   you pick (the base name is editable in the sidebar). For the other images, the sampling box is yours if they're the
   same size as the previewed one (otherwise a centered fit), and the
   background is auto-detected at the current tolerance (manual
   include/exclude clicks only affect the previewed image).
2. **Open Palette (.gpl)...** — optional. Sample palettes are included at
   `assets/palettes/pico-8.gpl` and `assets/palettes/apollo.gpl`.
3. In the sidebar, enable whichever **Tools** you want (Remove Background,
   Crop, Downscale) and adjust their settings; the source preview updates live.
4. **Convert** — runs the enabled steps on a background thread, in the order
   shown in the sidebar.
5. **Save Result As PNG...** — write the converted image out.

## Project layout

Each folder is one namespace (`src/core/graphics` -> `pixie::graphics`).
`core/` has no SDL or UI code; `app/` builds the program on top of it.
See [Architecture.md](Architecture.md) for layers, flows, threading and conventions.

```
src/
  main.cpp
  core/
    color/        pixie::color        Rgba, Lab, CIEDE2000, Palette (.gpl)
    graphics/     pixie::graphics     Image, Mask + background detection, Crop,
                                      Resize/thumbnails, PaletteRemap
    formats/      pixie::formats      Load/save images (stb), Aseprite reader/writer
    audio/        pixie::audio        AudioClip decoding (miniaudio, stb_vorbis), waveform
    library/      pixie::library      Asset scanning/pairing, variation groups
    spritesheet/  pixie::spritesheet  Atlas packing + layout JSON
  app/            pixie::app          Application (window, main loop), AppContext
    views/        pixie::views        LibraryView, PreviewPanel, ConverterView,
                                      SamplingBoxEditor
    services/     pixie::services     Settings, ProjectStore, ThumbnailCache,
                                      AudioPlayer, ExternalTools, ProjectExport,
                                      ConversionPipeline, BatchConversion
    util/         pixie::util         Text formatting, dialogs, textures, ImGui helpers
assets/
  source/      Sample source image
  palettes/    Sample .gpl palettes
  converted/   Default save-to location
```

## License

MIT — see [LICENSE](LICENSE).
