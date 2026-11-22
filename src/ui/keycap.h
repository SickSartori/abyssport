/* keycap.c: a key's or a mouse button's picture for the game's button glyphs on a keyboard and mouse */
#ifndef ABYSS_KEYCAP_H
#define ABYSS_KEYCAP_H
#include <stdint.h>
/* draws into `img` (w x h RGBA8, straight alpha, cleared first): with `mouse` 0 a key cap named `label` ("\1".."\4"
 * the arrows up, down, left, right), else a mouse with that button in gold (1 left, 2 right, 3 middle, 9 the whole
 * mouse: looking). 0: nothing drawn (no label, too small, or the launcher's letters missing) -- keep the game's own. */
int keycap_draw(const char *label, int mouse, int w, int h, uint8_t *img);
#endif
