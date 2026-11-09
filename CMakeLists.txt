cmake_minimum_required(VERSION 3.23)

project(Pixie LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# Dependencies (vcpkg manifest mode — see vcpkg.json)
# ---------------------------------------------------------------------------
find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h" REQUIRED)

# ---------------------------------------------------------------------------
# Pixie
# ---------------------------------------------------------------------------
add_executable(Pixie
  src/main.cpp
  src/app/AppContext.cpp
  src/app/AppContext.h
  src/app/Application.cpp
  src/app/Application.h
  src/app/services/AudioPlayer.cpp
  src/app/services/AudioPlayer.h
  src/app/services/BatchConversion.cpp
  src/app/services/BatchConversion.h
  src/app/services/ConversionPipeline.cpp
  src/app/services/ConversionPipeline.h
  src/app/services/ExternalTools.cpp
  src/app/services/ExternalTools.h
  src/app/services/ProjectStore.cpp
  src/app/services/ProjectStore.h
  src/app/services/Settings.cpp
  src/app/services/Settings.h
  src/app/services/SpriteSheetExport.cpp
  src/app/services/SpriteSheetExport.h
  src/app/services/ThumbnailCache.cpp
  src/app/services/ThumbnailCache.h
  src/app/util/Dialogs.cpp
  src/app/util/Dialogs.h
  src/app/util/Gui.cpp
  src/app/util/Gui.h
  src/app/util/Platform.h
  src/app/util/Text.cpp
  src/app/util/Text.h
  src/app/util/Texture.cpp
  src/app/util/Texture.h
  src/app/views/ConverterView.cpp
  src/app/views/ConverterView.h
  src/app/views/LibraryView.cpp
  src/app/views/LibraryView.h
  src/app/views/PreviewPanel.cpp
  src/app/views/PreviewPanel.h
  src/app/views/SamplingBoxEditor.cpp
  src/app/views/SamplingBoxEditor.h
  src/core/audio/AudioClip.cpp
  src/core/audio/AudioClip.h
  src/core/audio/StbVorbis.cpp
  src/core/color/Color.cpp
  src/core/color/Color.h
  src/core/color/Palette.cpp
  src/core/color/Palette.h
  src/core/formats/Aseprite.cpp
  src/core/formats/Aseprite.h
  src/core/formats/BinaryFile.cpp
  src/core/formats/BinaryFile.h
  src/core/formats/ImageFile.cpp
  src/core/formats/ImageFile.h
  src/core/graphics/Crop.cpp
  src/core/graphics/Crop.h
  src/core/graphics/Image.cpp
  src/core/graphics/Image.h
  src/core/graphics/Mask.cpp
  src/core/graphics/Mask.h
  src/core/graphics/PaletteRemap.cpp
  src/core/graphics/PaletteRemap.h
  src/core/graphics/Resize.cpp
  src/core/graphics/Resize.h
  src/core/library/Asset.cpp
  src/core/library/Asset.h
  src/core/library/VariationGroups.cpp
  src/core/library/VariationGroups.h
  src/core/spritesheet/SpriteSheet.cpp
  src/core/spritesheet/SpriteSheet.h
)

target_include_directories(Pixie PRIVATE src ${Stb_INCLUDE_DIR} ${MINIAUDIO_INCLUDE_DIRS})

target_link_libraries(Pixie PRIVATE
  SDL3::SDL3
  imgui::imgui
  nfd::nfd
)

if(MSVC)
  target_compile_options(Pixie PRIVATE /W4 /utf-8)
  target_compile_definitions(Pixie PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(Pixie PRIVATE -Wall -Wextra)
  find_package(Threads REQUIRED)
  target_link_libraries(Pixie PRIVATE Threads::Threads)
endif()

if(WIN32)
  # GUI subsystem: no console window is allocated when launching Pixie.exe.
  set_target_properties(Pixie PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Pixie)

# Make the sample palettes available next to the built executable.
add_custom_command(TARGET Pixie POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:Pixie>/assets
)

# On Windows, vcpkg's applocal deployment (enabled by default via the vcpkg
# toolchain) copies SDL3.dll etc. next to Pixie.exe automatically after build.
