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

# ---------------------------------------------------------------------------
# Pixie
# ---------------------------------------------------------------------------
add_executable(Pixie
  src/main.cpp
  src/core/ColorMath.h
  src/core/ColorMath.cpp
  src/core/Palette.h
  src/core/Palette.cpp
  src/core/Image.h
  src/core/Image.cpp
  src/core/Mask.h
  src/core/Mask.cpp
  src/core/Crop.h
  src/core/Crop.cpp
  src/core/Downscale.h
  src/core/Downscale.cpp
  src/core/Converter.h
  src/core/Converter.cpp
  src/app/Application.h
  src/app/Application.cpp
  src/app/TextureHelper.h
  src/app/TextureHelper.cpp
)

target_include_directories(Pixie PRIVATE src ${Stb_INCLUDE_DIR})

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
