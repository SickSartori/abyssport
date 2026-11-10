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
    message(FATAL_ERROR "Use a Visual Studio generator (the 'pc-x64' preset): the game's recompiled code is built "
                        "by clang-cl and the rest by MSVC, and only that generator picks a compiler per target.")
endif()

# The game's code, recompiled from the player's own copy: never in the repository (game/ is ignored).
set(ABYSS_RECOMP_DIR "${CMAKE_SOURCE_DIR}/game/library/decrypted/PCSA00029/recomp" CACHE PATH
    "The recompiler's output for PCSA00029 1.00: game.c, game.h and game_part*.c")
# shortcut: rt.h is read from the ignored Nativ checkout; adapt it into src/ (with its THIRD_PARTY.md row) once
# the list of what abyssport takes from Nativ is settled.
set(ABYSS_RT_DIR "${CMAKE_SOURCE_DIR}/external/nativ/recompiler" CACHE PATH
    "The folder with rt.h, the header the recompiled code includes")

file(GLOB ABYSS_GAME_SOURCES CONFIGURE_DEPENDS "${ABYSS_RECOMP_DIR}/game.c" "${ABYSS_RECOMP_DIR}/game_part*.c")
if(NOT ABYSS_GAME_SOURCES OR NOT EXISTS "${ABYSS_RT_DIR}/rt.h")
    message(FATAL_ERROR "The game's recompiled code is missing: no game.c in ${ABYSS_RECOMP_DIR}, or no rt.h in "
                        "${ABYSS_RT_DIR}. It is made from your own copy of the game; set ABYSS_RECOMP_DIR and "
                        "ABYSS_RT_DIR if it is somewhere else.")
endif()

# The game: 32,000 functions of ARM code as C, 425 MB of it. Built by clang-cl (the "C++ Clang tools" component of
# Visual Studio): the code uses GNU C (statement expressions, __atomic_*, __thread), and MSVC's optimiser does not
# finish a single one of its files in an hour and a half where Clang takes 20 seconds.
add_library(abyssgame STATIC ${ABYSS_GAME_SOURCES})
set_target_properties(abyssgame PROPERTIES VS_PLATFORM_TOOLSET ClangCL)
target_include_directories(abyssgame PRIVATE "${ABYSS_RT_DIR}")
# No fused multiply-adds: the Vita's VFP rounds the product before it adds (rt.h). No warnings: generated code.
target_compile_options(abyssgame PRIVATE /clang:-ffp-contract=off -w /MP)
