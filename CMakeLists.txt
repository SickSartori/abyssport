cmake_minimum_required(VERSION 3.25)
project(abyssport VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MINGW OR NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "abyssport is built with llvm-mingw for 64-bit Windows: use the 'pc-x64' preset "
                        "(tools/get_toolchain.py fetches the compiler). The Vita's 4 GiB of address space is one "
                        "block of the process's memory, and the game's recompiled code is GNU C.")
endif()

# Optimised without -DNDEBUG, as Project Nativ builds: the runtime's own checks stay in.
set(CMAKE_C_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
# One program, nothing beside it but FFmpeg's DLLs: the C and C++ libraries and winpthreads are linked in.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

# The game's code, recompiled from the player's own copy: never in the repository (game/ is ignored).
set(ABYSS_RECOMP_DIR "${CMAKE_SOURCE_DIR}/game/library/decrypted/PCSA00029/recomp" CACHE PATH
    "The recompiler's output for PCSA00029 1.00 (tools/make_game.py): game*.c, the libraries' mod_*.c, the .img images")
file(GLOB ABYSS_GAME_SOURCES CONFIGURE_DEPENDS "${ABYSS_RECOMP_DIR}/game.c" "${ABYSS_RECOMP_DIR}/game_part*.c"
                                               "${ABYSS_RECOMP_DIR}/mod_*.c")
if(NOT ABYSS_GAME_SOURCES)
    message(FATAL_ERROR "The game's recompiled code is missing: no game.c in ${ABYSS_RECOMP_DIR}. It is made from "
                        "your own copy of the game by tools/make_game.py; set ABYSS_RECOMP_DIR if it is somewhere else.")
endif()

# The POSIX names Windows spells differently, in front of every C file of ours (src/platform/win32_compat.h):
# the runtime and the recompiled code are written for POSIX. 64-bit file offsets; mingw-w64's localtime_r.
set(ABYSS_COMPAT -D_FILE_OFFSET_BITS=64 -include "${CMAKE_SOURCE_DIR}/src/platform/win32_compat.h")

# The game: 32,000 functions of ARM code as C, 425 MB of it, and Sony's libraries recompiled the same way: libc and
# libfios2 from the game, libgxm, libgpu and libscemp4 from the firmware. An object library: each library registers
# its tables from a constructor nothing refers to, which a static library would leave out of the program.
add_library(abyssgame OBJECT ${ABYSS_GAME_SOURCES})
target_include_directories(abyssgame PUBLIC src/recomp)
# No fused multiply-adds: the Vita's VFP rounds the product before it adds (rt.h). No warnings: generated code.
target_compile_options(abyssgame PRIVATE -ffp-contract=off -w ${ABYSS_COMPAT} -D_POSIX_THREAD_SAFE_FUNCTIONS)

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

# FFmpeg (LGPL 2.1 or later, this build of it): the H.264 decoder behind the game's films. A ready-made build for
# Windows with shared libraries, downloaded at configure time, pinned by release and hash; only libavcodec and
# libavutil are used, and their DLLs are copied beside the program.
FetchContent_Declare(ffmpeg
    URL https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-10-31-12-59/ffmpeg-n8.1.3-27-g8dca1586f4-win64-lgpl-shared-8.1.zip
    URL_HASH SHA256=5cef7bc48e513f3cee7c881e8b2999d835541edb1846c3e89443a67f8f8556ab
)
FetchContent_MakeAvailable(ffmpeg)

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
target_compile_options(imgui PRIVATE -w)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

# LibAtrac9 (MIT, third_party/libatrac9): the decoder of the game's ATRAC9 audio.
file(GLOB ABYSS_ATRAC9_SOURCES CONFIGURE_DEPENDS third_party/libatrac9/*.c)
add_library(atrac9 STATIC ${ABYSS_ATRAC9_SOURCES})
target_compile_options(atrac9 PRIVATE -w)

# The touch card's input icons (third_party/kenney-prompts, CC0) as bytes the overlay includes.
file(READ third_party/kenney-prompts/prompts.png ABYSS_PROMPTS HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ABYSS_PROMPTS "${ABYSS_PROMPTS}")
file(WRITE "${CMAKE_BINARY_DIR}/generated/prompts_png.h" "static const unsigned char prompts_png[] = {${ABYSS_PROMPTS}};\n")

# The program: the host of the recompiled code and the Vita's system: kernel, files, graphics, audio, input.
# The runtime is adapted from Project Nativ.
set(ABYSS_C_SOURCES
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
    src/platform/win32_compat.c
    src/ui/window.c
)
add_executable(abyss ${ABYSS_C_SOURCES} src/ui/overlay.cpp)
target_include_directories(abyss PRIVATE
    src/audio src/core src/debug src/gfx src/gfx/fsr src/input src/media src/platform src/ui
    "${CMAKE_SOURCE_DIR}"                       # ("third_party/...")
    "${CMAKE_BINARY_DIR}/generated"
    "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}"
    "${ffmpeg_SOURCE_DIR}/include"
)
# No warnings for now: the adapted code is kept as it is upstream. (_POSIX_THREAD_SAFE_FUNCTIONS: C only -- with it
# mingw-w64 defines the localtime_r that Dear ImGui's headers also define.)
target_compile_options(abyss PRIVATE -w ${ABYSS_COMPAT})
set_source_files_properties(${ABYSS_C_SOURCES} PROPERTIES COMPILE_DEFINITIONS _POSIX_THREAD_SAFE_FUNCTIONS)
# The settings the game plays with, as Project Nativ's pack for it sets them (tools/PCSA00029.toml RUN_ENV).
set_property(SOURCE src/core/main.c APPEND PROPERTY COMPILE_DEFINITIONS
    "RECOMP_BUILTIN_ENV=\"RECOMP_GAME_OPTIONS=fov,view_distance RECOMP_SRGB=1 RECOMP_ASPECT_HOOK=1 RECOMP_HUD_DRAWS=2d RECOMP_TARGET_SCALE=720x408:1.3333 RECOMP_CAMERA_LAMP=1 RECOMP_FSR_SUPPORTED=1\"")
# The films: sceAvcdec through libavcodec.
set_property(SOURCE src/media/video.c APPEND PROPERTY COMPILE_DEFINITIONS RECOMP_AVCODEC)
target_link_libraries(abyss PRIVATE abyssgame atrac9 imgui zlibstatic SDL3::SDL3-static
    "${ffmpeg_SOURCE_DIR}/lib/libavcodec.dll.a" "${ffmpeg_SOURCE_DIR}/lib/libavutil.dll.a"
    pthread winmm ws2_32 bcrypt)
file(GLOB ABYSS_FFMPEG_DLLS "${ffmpeg_SOURCE_DIR}/bin/avcodec-*.dll" "${ffmpeg_SOURCE_DIR}/bin/avutil-*.dll"
                            "${ffmpeg_SOURCE_DIR}/bin/swresample-*.dll")
add_custom_command(TARGET abyss POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ABYSS_FFMPEG_DLLS} "$<TARGET_FILE_DIR:abyss>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ffmpeg_SOURCE_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:abyss>/FFmpeg-LICENSE.txt")

# The check: the game starts and draws its first frames -- headless, on the software renderer, which is the reference
# (81009ffc: module_start of 1.00). The game loads its front end first.
enable_testing()
add_test(NAME abyss_first_frames COMMAND abyss game.img 81009ffc 100000000000 WORKING_DIRECTORY "${ABYSS_RECOMP_DIR}")
set_tests_properties(abyss_first_frames PROPERTIES
    ENVIRONMENT "RECOMP_MODULES=libc.img,libgxm_es4.img,libgpu_es4.img,libfios2.img,libscemp4.img;RECOMP_GFX=soft;RECOMP_SHADERS=interp;RECOMP_NO_MSPACE=1;RECOMP_SRGB=1;RECOMP_TITLE=PCSA00029;RECOMP_HEADLESS=1;RECOMP_FS=fs;RECOMP_LOG=0;RECOMP_SHOT=3;RECOMP_SHOT_LAST=3"
    PASS_REGULAR_EXPRESSION "frame 3 shot: the last one asked for"
    TIMEOUT 300)
