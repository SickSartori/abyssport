// abyssport's launcher as the game's menu (F1): launcher.cpp built into the runtime with ABYSS_LAUNCHER_IN_GAME, its
// pages drawn by src/ui/overlay.cpp over the paused game, in the overlay's ImGui context and on its GPU device.
#pragma once
#include <SDL3/SDL.h>

// Once, after the overlay's ImGui context is made: the launcher's letters into its atlas, its pictures onto `device`.
// `apply(key)`: a setting changed that the game takes at once when told (render_scale, fullscreen, osd).
void launcher_game_init(SDL_GPUDevice* device, void (*apply)(const char* key));
// The menu opened: its first page, Resume selected.
void launcher_game_open();
// One frame of the menu, inside the overlay's ImGui frame: 0 still open, 1 Resume, 2 Back to the launcher (or Quit),
// 3 Quit the game and the launcher that started it.
int launcher_game_frame(int width, int height);
// An event before ImGui sees it: true when taken (the key for an action being changed).
bool launcher_game_event(const SDL_Event* event);
