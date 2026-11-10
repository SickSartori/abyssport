@echo off
rem Plays the game from the build tree (the pc-x64 preset, RelWithDebInfo): the game's code and data are in
rem game\library, made from your own copy. Arguments are passed on (a frame count ends the run after that many).
setlocal
set RECOMP_MODULES=libc.img,libgxm_es4.img,libgpu_es4.img,libfios2.img,libscemp4.img
set RECOMP_TITLE=PCSA00029
set RECOMP_GAME_NAME=Uncharted: Golden Abyss
set RECOMP_FS=fs
rem The shader recipes that ship (res\shaders.txt): the game's shaders are built as it starts, not at their first use.
set RECOMP_SHADER_RECIPES_SHIPPED=%~dp0res\shaders.txt
rem The intro movie needs an H.264 decoder the build does not have yet: with this the game skips it and goes to its
rem title (without it the screen stays black). To be removed with the decoder.
set RECOMP_NO_MSPACE=1
if not defined RECOMP_LOG set RECOMP_LOG=0
cd /d "%~dp0game\library\decrypted\PCSA00029\recomp" || exit /b 1
"%~dp0build\pc-x64\RelWithDebInfo\abyss.exe" game.img 81009ffc 100000000000 %*
