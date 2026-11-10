# The compiler: llvm-mingw (Clang with its own C and C++ libraries), from toolchain/ (tools/get_toolchain.py) or
# from ABYSS_TOOLCHAIN. One compiler for the runtime and for the game's recompiled code, the one Project Nativ
# builds with, and one that can be given to players: Visual Studio's can't.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(DEFINED ENV{ABYSS_TOOLCHAIN})
    set(ABYSS_TOOLCHAIN "$ENV{ABYSS_TOOLCHAIN}")
else()
    get_filename_component(ABYSS_TOOLCHAIN "${CMAKE_CURRENT_LIST_DIR}/../toolchain" ABSOLUTE)
endif()
set(_llvm "${ABYSS_TOOLCHAIN}/llvm-mingw/bin")
if(NOT EXISTS "${_llvm}/x86_64-w64-mingw32-clang.exe")
    message(FATAL_ERROR "No compiler at ${_llvm}: run  python tools/get_toolchain.py  first (it downloads llvm-mingw "
                        "and Ninja into toolchain/, nothing is installed).")
endif()
set(CMAKE_C_COMPILER "${_llvm}/x86_64-w64-mingw32-clang.exe")
set(CMAKE_CXX_COMPILER "${_llvm}/x86_64-w64-mingw32-clang++.exe")
set(CMAKE_RC_COMPILER "${_llvm}/x86_64-w64-mingw32-windres.exe")
set(CMAKE_MAKE_PROGRAM "${ABYSS_TOOLCHAIN}/ninja.exe" CACHE FILEPATH "" FORCE)
