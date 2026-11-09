# Pixie architecture

Pixie is a desktop asset viewer for pixel-art game projects. It browses a folder of
images and audio, pairs each base image with its converted `.aseprite`, groups
variations, organizes assets into projects, and converts or exports them.

This file describes how the code is organized. Keep it in sync with every
structural change (new folder, namespace, class, flow or persisted file).

## Layers

```
main.cpp
   │
   ▼
app/            Application, AppContext          ── owns SDL, ImGui, the main loop
   ├── views/      screens and panels (ImGui)     ── read/write AppContext, call services
   ├── services/   stateful helpers + workflows   ── no ImGui
   └── util/       small SDL/ImGui/text helpers
   │
   ▼
core/           pure logic, no SDL and no UI      ── testable on its own
   color → graphics → formats → library / spritesheet;  audio
```

Dependency rules:

- `core/` never includes anything from `app/`, SDL or ImGui.
- `services/` may use SDL (textures, audio, message boxes) and nfd file dialogs, but not ImGui.
- `views/` are the only place that draws UI. They talk to each other through
  `AppContext` and direct references set up by `Application`.
- Each folder is one namespace: `src/core/graphics` → `pixie::graphics`,
  `src/app/views` → `pixie::views`, and so on. `app/` itself is `pixie::app`.

## Folders and namespaces

### core/

| Folder | Namespace | Contents |
|---|---|---|
| `color/` | `pixie::color` | `Rgba` (8-bit RGBA), `Lab`, `ToLab`, `Ciede2000`; `Palette` (GIMP `.gpl`) |
| `graphics/` | `pixie::graphics` | `Image` (pixel buffer, no IO); `Mask` + `DetectBackground`/`ToggleRegion`/`ApplyMask`; `CropRect`, `CropImage`, `CenteredRectWithAspect`; `ResizeNearest`, `MakeThumbnail`; `RemapToPalette`, `BinarizeAlpha`, `RemoveTransparency` |
| `formats/` | `pixie::formats` | `LoadImageFile`, `SavePng`, `SaveAseprite` (stb_image); Aseprite reader (all frames, layers, tags, linked/compressed cels) and writer; `ReadWholeFile`/`WriteWholeFile` |
| `audio/` | `pixie::audio` | `AudioClip`, `LoadClip` (WAV/MP3/FLAC via miniaudio, Ogg via stb_vorbis), `ComputeWaveform` |
| `library/` | `pixie::library` | `Asset`, `AssetKind`, `ScanFolder`; `VariationGroupKey`, `BuildGridTiles` |
| `spritesheet/` | `pixie::spritesheet` | `Build`: packs Aseprite frames into an atlas + layout JSON |

### app/

| Folder | Namespace | Contents |
|---|---|---|
| `app/` | `pixie::app` | `Application` (window, ImGui, main loop, screen switch); `AppContext` (shared state); `Screen` enum |
| `views/` | `pixie::views` | `LibraryView` (folder picker, grid, filters, projects bar); `PreviewPanel` (side panel); `ConverterView` (conversion screen); `SamplingBoxEditor` (Downsize box drag/resize) |
| `services/` | `pixie::services` | `Settings`, `ProjectStore`, `ThumbnailCache`, `AudioPlayer`, `ExternalTools` (Aseprite/Audacity), `SpriteSheetExport`, `ConversionPipeline` (`ConvertImage`), `BatchConversion` (`RunBatch`) |
| `util/` | `pixie::util` | `Text` (formatting, UTF-8 paths), `Dialogs` (overwrite prompt), `Texture` (SDL upload), `Gui` (fullscreen window, fit helpers), `Platform.h` (`PIXIE_NSTR`, `NativeString`) |

## Object graph

`Application` owns everything; views keep references.

```
Application
 ├── AppContext context_      window, renderer, settings, projects, thumbnails,
 │                            assets, selectedKeys, statusMessage, screen
 ├── ConverterView converter_ (context)
 ├── PreviewPanel preview_    (context, converter)
 └── LibraryView library_     (context, converter, preview)
```

`AppContext::screen` decides what `Application::RenderCurrentScreen` draws:
`FolderPicker` and `Library` (LibraryView, with PreviewPanel inside it) or `Converter`.

## Key concepts

- **Asset**: one logical item. An image asset joins `hero.png` (base) and
  `hero.aseprite` (converted) from the same folder. Each audio file is its own asset.
- **Asset key**: `lowercase folder + "/" + lowercase name`, plus the extension for
  audio. Projects, the selection and the thumbnail cache all refer to assets by key,
  so renaming an asset must update all three (see `PreviewPanel::RenameAsset`).
- **Variation group**: same kind and folder, and the same name up to the first digit
  (`Armor01a`, `Armor02b` → `Armor`). The grouping only affects the view, never projects.
- **Project**: a named set of asset keys.

## Main flows

- **Startup**: `Application::Init` creates the window, loads the default palette and
  settings, then `AppContext::OpenSavedLibrary`, or shows the folder picker.
- **Library grid**: `LibraryView::VisibleAssetIndices` applies the project, type,
  conversion and search filters. `library::BuildGridTiles` collapses variation groups,
  and `RenderGrid` draws only the visible rows (ImGui list clipper). Thumbnails decode
  lazily within a per-frame time budget (`ThumbnailCache`).
- **Preview**: clicking a tile calls `PreviewPanel::Open`, which loads the base image,
  every Aseprite frame (animated with the real frame durations), or the audio clip and
  its waveform (`AudioPlayer`).
- **Single conversion**: `ConverterView::ConvertPreviewedImage` copies its inputs and
  runs `services::ConvertImage` on a `std::async` worker. `Update()` collects the result.
  Pipeline order: crop → remove transparency → background mask → palette remap →
  nearest resize → binarize alpha.
- **Batch conversion**: `ConverterView::ConvertAllImages` fills a `services::BatchJob`
  and runs `RunBatch` on a worker. Progress goes through `BatchProgress`, whose
  `processed` counter is atomic.
- **Spritesheet export**: `services::ExportSpriteSheet` collects the active project's
  `.aseprite` files and calls `spritesheet::Build`. It writes `<name>.png` plus
  `<name>_layout.json`, which the Godot importer reads.

## Threading

Only the converter uses a background thread (one `std::future` at a time). While
`converting_` is true:

- The UI never reads `result_`; it uses the `resultWidth_`/`resultHeight_` snapshot.
- The worker only touches data copied into it, plus `result_` (single conversion) or
  `batchProgress_` (batch).

`RemapToPalette` splits its rows across hardware threads internally. Everything else
runs on the UI thread.

## Persistence

| File | Owner | Contents |
|---|---|---|
| `%APPDATA%/Pixie/Pixie/settings.txt` | `services::Settings` | `library_root`, `tile_size`, `audacity_path` |
| `<library>/pixie-projects.txt` | `services::ProjectStore` | `project=<name>` followed by `asset=<key>` lines |

## Conventions

- Comments are a single line. Name things so that most code needs no comment.
- Use descriptive names: no single-letter variables outside tiny loops and math formulas.
- Long `Render` functions are split into named `Render*`/`Draw*` helpers.
- All visible UI text is in English.
- The app icon lives in `resources/icon/` (`pixie.png`, `pixie.ico`), generated by `python tools/make_icon.py`.
  The `.ico` is embedded in the exe through `resources/pixie.rc`; the PNG is copied next to the exe and set as the window icon.
- Add every new `.cpp`/`.h` to `add_executable` in `CMakeLists.txt`.

## Extending

- **New asset kind**: add it to `library::AssetKind` before `Count`, give it a label in
  `KindLabel`, and detect it in `ScanFolder`. The type filter picks it up automatically.
  Give it tile drawing in `LibraryView::DrawTileImage` and a preview in `PreviewPanel`.
- **New screen**: add a value to `app::Screen`, a view class in `views/` owned by
  `Application`, and a case in `RenderCurrentScreen`.
- **New conversion step**: add the setting to `ConversionSettings` and the step to
  `services::ConvertImage`, which serves both single and batch conversion.
