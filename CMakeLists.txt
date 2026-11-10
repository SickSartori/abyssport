cmake_minimum_required(VERSION 3.25)
project(abyssport VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT everywhere: the program must not depend on redistributables.
# Read when targets are created, so it has to come before them.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The PC build must be 64-bit (x64): the Vita's 4 GiB of address space is one block of the "
                        "process's memory. Use the 'pc-x64' preset.")
endif()
if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    message(FATAL_ERROR "Use a Visual Studio generator (the 'pc-x64' preset): the game's recompiled code and the "
                        "runtime are built by clang-cl, the libraries by MSVC, and only that generator picks a "
                        "compiler per target.")
endif()

# The game's code, recompiled from the player's own copy: never in the repository (game/ is ignored).
set(ABYSS_RECOMP_DIR "${CMAKE_SOURCE_DIR}/game/library/decrypted/PCSA00029/recomp" CACHE PATH
    "The recompiler's output for PCSA00029 1.00: game*.c, the libraries' mod_*.c, and their .img memory images")
file(GLOB ABYSS_GAME_SOURCES CONFIGURE_DEPENDS "${ABYSS_RECOMP_DIR}/game.c" "${ABYSS_RECOMP_DIR}/game_part*.c"
                                               "${ABYSS_RECOMP_DIR}/mod_*.c")
if(NOT ABYSS_GAME_SOURCES)
    message(FATAL_ERROR "The game's recompiled code is missing: no game.c in ${ABYSS_RECOMP_DIR}. It is made from "
                        "your own copy of the game; set ABYSS_RECOMP_DIR if it is somewhere else.")
endif()

# The game: 32,000 functions of ARM code as C, 425 MB of it. Built by clang-cl (the "C++ Clang tools" component of
# Visual Studio): the code uses GNU C (statement expressions, __atomic_*, __thread), and MSVC's optimiser does not
# finish a single one of its files in an hour and a half where Clang takes 20 seconds. With it, Sony's libraries
# recompiled the same way: libc and libfios2 from the game, libgxm, libgpu and libscemp4 from the firmware. An object
# library: each library registers its tables from a constructor nothing refers to, which a static library would
# leave out of the program.
add_library(abyssgame OBJECT ${ABYSS_GAME_SOURCES})
set_target_properties(abyssgame PROPERTIES VS_PLATFORM_TOOLSET ClangCL)
target_include_directories(abyssgame PUBLIC src/recomp)
# No fused multiply-adds: the Vita's VFP rounds the product before it adds (rt.h). No warnings: generated code.
target_compile_options(abyssgame PRIVATE /clang:-ffp-contract=off -w /MP)

include(FetchContent)

# zlib (zlib licence): the game's archives (PSARC) are deflated. Downloaded at configure time, pinned by version
# and hash, linked statically.
FetchContent_Declare(zlib
    URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

# SDL3 (zlib licence): the window, the GPU API the renderer draws through, audio output, controllers and sensors.
# Downloaded at configure time, pinned by version and hash, linked statically.
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz
    URL_HASH SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# Dear ImGui (MIT, third_party/imgui): the in-game overlay's interface, drawn through SDL's GPU API.
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl3.cpp
    third_party/imgui/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

# LibAtrac9 (MIT, third_party/libatrac9): the decoder of the game's ATRAC9 audio.
file(GLOB ABYSS_ATRAC9_SOURCES CONFIGURE_DEPENDS third_party/libatrac9/*.c)
add_library(atrac9 STATIC ${ABYSS_ATRAC9_SOURCES})
set_target_properties(atrac9 PROPERTIES VS_PLATFORM_TOOLSET ClangCL)
target_compile_options(atrac9 PRIVATE -w)
# (its header marks the functions dllimport under MSVC unless it is told it is being built)
target_compile_definitions(atrac9 PUBLIC COMPILING_DLL)

# The touch card's input icons (third_party/kenney-prompts, CC0) as bytes the overlay includes.
file(READ third_party/kenney-prompts/prompts.png ABYSS_PROMPTS HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ABYSS_PROMPTS "${ABYSS_PROMPTS}")
file(WRITE "${CMAKE_BINARY_DIR}/generated/prompts_png.h" "static const unsigned char prompts_png[] = {${ABYSS_PROMPTS}};\n")

# The program: the host of the recompiled code and the Vita's system: kernel, files, graphics, audio, input.
# Built by clang-cl too: it shares rt.h with the game's code, and the runtime, adapted from Project Nativ, is GNU C
# over POSIX (src/platform/posix).
add_executable(abyss
    src/audio/ngs.c
    src/core/apputil.c
    src/core/fios.c
    src/core/hle.c
    src/core/hooks.c
    src/core/io.c
    src/core/kernel.c
    src/core/libc.c
    src/core/main.c
    src/core/metrics.c
    src/core/net.c
    src/core/psarc.c
    src/core/settings.c
    src/core/sysmem.c
    src/debug/gfx_capture.c
    src/debug/gfx_coverage.c
    src/debug/memscan.c
    src/debug/stallprof.c
    src/debug/waitstats.c
    src/game/hooks.c
    src/gfx/display.c
    src/gfx/fsr/fsr.c
    src/gfx/gfx_dispatch.c
    src/gfx/gfx_gpu.c
    src/gfx/gfx_soft.c
    src/gfx/gpu.c
    src/gfx/gpulock.c
    src/gfx/guestwatch.c
    src/gfx/gxm_sony.c
    src/gfx/gxp_index.c
    src/gfx/post.c
    src/gfx/ssao.c
    src/gfx/usse.c
    src/gfx/usse_spirv.c
    src/input/gesture.c
    src/input/motion.c
    src/media/camera.c
    src/media/jpeg.c
    src/media/video.c
    src/platform/platform_win32.c
    src/platform/posix.c
    src/ui/overlay.cpp
    src/ui/window.c
)
set_target_properties(abyss PROPERTIES VS_PLATFORM_TOOLSET ClangCL)
target_include_directories(abyss PRIVATE
    src/audio src/core src/debug src/gfx src/gfx/fsr src/input src/media src/platform src/platform/posix src/ui
    "${CMAKE_SOURCE_DIR}"                       # ("third_party/...")
    "${CMAKE_BINARY_DIR}/generated"
    "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}"
)
# compat.h in front of every file: the POSIX file calls under Windows' names (src/platform/posix). No warnings
# for now: the adapted code is kept as it is upstream.
target_compile_options(abyss PRIVATE /FIcompat.h -w /MP)
target_compile_definitions(abyss PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
# The settings the game plays with, as Project Nativ's pack for it sets them (packs/PCSA00029/pack.toml RUN_ENV).
set_source_files_properties(src/core/main.c PROPERTIES COMPILE_DEFINITIONS
    "RECOMP_BUILTIN_ENV=\"RECOMP_GAME_OPTIONS=fov,view_distance RECOMP_SRGB=1 RECOMP_ASPECT_HOOK=1 RECOMP_HUD_DRAWS=2d RECOMP_TARGET_SCALE=720x408:1.3333 RECOMP_CAMERA_LAMP=1 RECOMP_FSR_SUPPORTED=1\"")
target_link_libraries(abyss PRIVATE abyssgame atrac9 imgui zlibstatic SDL3::SDL3-static bcrypt)

# The check: the game starts and draws its first frames -- headless, on the software renderer, which is the reference
# (81009ffc: module_start of 1.00). About a minute: the game loads its front end first.
enable_testing()
add_test(NAME abyss_first_frames COMMAND abyss game.img 81009ffc 100000000000 WORKING_DIRECTORY "${ABYSS_RECOMP_DIR}")
set_tests_properties(abyss_first_frames PROPERTIES
    ENVIRONMENT "RECOMP_MODULES=libc.img,libgxm_es4.img,libgpu_es4.img,libfios2.img,libscemp4.img;RECOMP_GFX=soft;RECOMP_SHADERS=interp;RECOMP_NO_MSPACE=1;RECOMP_SRGB=1;RECOMP_TITLE=PCSA00029;RECOMP_HEADLESS=1;RECOMP_FS=fs;RECOMP_LOG=0;RECOMP_SHOT=3;RECOMP_SHOT_LAST=3"
    PASS_REGULAR_EXPRESSION "frame 3 shot: the last one asked for"
    TIMEOUT 300)
