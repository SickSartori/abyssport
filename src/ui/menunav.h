/* The game's own menus by the pad and the keyboard: the touchable buttons a game hook finds on screen each frame
 * (Uncharted's: src/game/hooks.c uc_menu_scan), and window.c moving a highlight among them, a tap or a swipe for a
 * press. The menus of a Vita game are played by touch; these are the same touches, made by the pad. */
#pragma once
#include <stdint.h>
enum { MENU_BUTTON, MENU_CAROUSEL, MENU_BACK, MENU_SCREEN, MENU_PART };   /* a tap; a strip swiped left/right; the way back; the whole
                                                                        * screen; a carousel's own item (not a stop: how wide it is) */
typedef struct { float x, y, w, h; uint32_t id; int kind; } MenuItem;   /* x, y, w, h on the Vita's screen (960 x 544) */
#define MENU_MAX 64
#ifdef __cplusplus
extern "C" {
#endif
void window_menu_items(const MenuItem *items, int n);   /* (the game's thread, each frame) what a menu has on screen now; 0 none */
#ifdef __cplusplus
}
#endif
