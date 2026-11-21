/*
 * keycap.c: the picture of a key or a mouse button, drawn into an RGBA8 image, for the game's button glyphs played on
 * a keyboard and mouse (gfx_gpu.c: the prompt textures). A key is a dark rounded cap with a light rim, as the game's own
 * buttons are, and its name in the launcher's letters (fonts/BarlowSemiCondensed-SemiBold.ttf beside the launcher, as
 * the in-game menu loads them), the arrows drawn; a mouse is its outline with the button in gold, as the launcher's
 * prompts show them. Shapes are sampled 4x4 a pixel, so they stay smooth at the 32-128 pixel sizes the game's glyphs have.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <SDL3/SDL.h>
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/imgui/imstb_truetype.h"
#include "keycap.h"

static stbtt_fontinfo font; static unsigned char *font_data; static int font_state;   /* 0 not tried, 1 loaded, -1 none */
static unsigned char *read_all(const char *path) {
	FILE *f = 0;
#ifdef _WIN32
	wchar_t w[1024]; if (MultiByteToWideChar(CP_UTF8, 0, path, -1, w, 1024)) f = _wfopen(w, L"rb");
#else
	f = fopen(path, "rb");
#endif
	if (!f) return 0;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	unsigned char *b = n > 0 ? malloc((size_t)n) : 0;
	if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = 0; }
	fclose(f);
	return b;
}
static int font_ready(void) {
	if (font_state) return font_state > 0;
	font_state = -1;
	char path[1024]; const char *dir = getenv("ABYSS_LAUNCHER_DIR");
	static const char file[] = "fonts/BarlowSemiCondensed-SemiBold.ttf";
	if (dir && *dir) { snprintf(path, sizeof path, "%s/%s", dir, file); font_data = read_all(path); }
	if (!font_data) { const char *base = SDL_GetBasePath(); if (base) { snprintf(path, sizeof path, "%s%s", base, file); font_data = read_all(path); } }
	if (font_data && stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0))) font_state = 1;
	else fprintf(stderr, "    keycap: no %s beside the launcher: the game's own button glyphs stay\n", file);
	return font_state > 0;
}

/* signed distance to a rounded box centred at (cx, cy), half sizes hx, hy, corner radius r (negative inside) */
static float box(float x, float y, float cx, float cy, float hx, float hy, float r) {
	float qx = fabsf(x - cx) - hx + r, qy = fabsf(y - cy) - hy + r;
	float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0, in = qx > qy ? qx : qy;
	return sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
}
static void blend(uint8_t *p, const uint8_t c[3], float a) {   /* `c` over the pixel with coverage a (straight alpha) */
	if (a <= 0) return; if (a > 1) a = 1;
	float da = p[3] / 255.0f, oa = a + da * (1 - a);
	for (int k = 0; k < 3; k++) p[k] = (uint8_t)((c[k] * a + p[k] * da * (1 - a)) / oa + 0.5f);
	p[3] = (uint8_t)(oa * 255 + 0.5f);
}
typedef float (*Shape)(float x, float y, const float *s);   /* <= 0 inside */
static void fill(uint8_t *img, int w, int h, Shape f, const float *s, const uint8_t c[3]) {
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		int n = 0;
		for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) n += f(x + (i + 0.5f) / 4, y + (j + 0.5f) / 4, s) <= 0;
		if (n) blend(img + 4 * (y * w + x), c, n / 16.0f);
	}
}
static float s_box(float x, float y, const float *s) { return box(x, y, s[0], s[1], s[2], s[3], s[4]); }
static float s_ring(float x, float y, const float *s) { return fabsf(box(x, y, s[0], s[1], s[2], s[3], s[4])) - s[5]; }
static float s_tri(float x, float y, const float *s) {   /* a triangle (s[0..5], either winding): inside when on the same side of all three edges */
	float lo = 1e9f, hi = -1e9f;
	for (int i = 0; i < 3; i++) {
		float ax = s[2 * i], ay = s[2 * i + 1], bx = s[2 * ((i + 1) % 3)], by = s[2 * ((i + 1) % 3) + 1];
		float e = ((bx - ax) * (y - ay) - (by - ay) * (x - ax)) / sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
		if (e < lo) lo = e; if (e > hi) hi = e;
	}
	return lo >= 0 ? -lo : hi <= 0 ? hi : 1;
}
static float s_mouse_button(float x, float y, const float *s) {   /* the mouse's body (s[0..4]) on the side s[5] (-1 left, 1 right) above y s[6] */
	if (box(x, y, s[0], s[1], s[2], s[3], s[4]) > 0 || y > s[6]) return 1;
	return (x - s[0]) * s[5] < 0 ? 1 : -1;
}

static const uint8_t CAP[3] = {38, 36, 34}, EDGE[3] = {150, 146, 138}, INK[3] = {240, 238, 232}, WHITE[3] = {240, 238, 232}, GOLD[3] = {226, 168, 44};

static void text(uint8_t *img, int w, int h, const char *label, float box_w, float box_h) {
	if (!font_ready()) return;
	float px = box_h * 0.62f, scale = stbtt_ScaleForPixelHeight(&font, px);
	int adv = 0, n = (int)strlen(label);
	for (int i = 0; i < n; i++) { int a, l; stbtt_GetCodepointHMetrics(&font, (unsigned char)label[i], &a, &l); adv += a; if (i + 1 < n) adv += stbtt_GetCodepointKernAdvance(&font, (unsigned char)label[i], (unsigned char)label[i + 1]); }
	if (adv * scale > box_w * 0.84f) { scale *= box_w * 0.84f / (adv * scale); px = scale / stbtt_ScaleForPixelHeight(&font, 1); }
	int asc, desc, gap; stbtt_GetFontVMetrics(&font, &asc, &desc, &gap);
	float x = (w - adv * scale) * 0.5f, base = h * 0.5f + (asc + desc) * scale * 0.5f - h * 0.03f;   /* (a cap's face sits a little high: its edge below) */
	for (int i = 0; i < n; i++) {
		int cp = (unsigned char)label[i], gw, gh, ox, oy, a, l;
		unsigned char *g = stbtt_GetCodepointBitmapSubpixel(&font, scale, scale, x - floorf(x), 0, cp, &gw, &gh, &ox, &oy);
		for (int y = 0; y < gh; y++) for (int xx = 0; xx < gw; xx++) {
			int X = (int)floorf(x) + ox + xx, Y = (int)floorf(base) + oy + y;
			if (X >= 0 && X < w && Y >= 0 && Y < h && g[y * gw + xx]) blend(img + 4 * (Y * w + X), INK, g[y * gw + xx] / 255.0f);
		}
		stbtt_FreeBitmap(g, 0);
		stbtt_GetCodepointHMetrics(&font, cp, &a, &l); x += a * scale;
		if (i + 1 < n) x += stbtt_GetCodepointKernAdvance(&font, cp, (unsigned char)label[i + 1]) * scale;
	}
}

int keycap_draw(const char *label, int mouse, int w, int h, uint8_t *img) {
	if (w < 8 || h < 8) return 0;
	memset(img, 0, (size_t)w * h * 4);
	const float m = (w < h ? w : h) * 0.06f, cx = w * 0.5f, cy = h * 0.5f;
	if (mouse) {   /* the mouse: its outline, the button (1 left, 2 right, 3 middle; 9 the whole mouse, moved) in gold */
		const float hx = (w < h ? w : h) * 0.27f, hy = h * 0.5f - m, r = hx * 0.95f, split = cy - hy * 0.1f, line = hx * 0.12f;
		float body[7] = {cx, cy, hx, hy, r, mouse == 2 ? 1.0f : -1.0f, split};
		if (mouse == 9) fill(img, w, h, s_box, body, GOLD);
		else if (mouse == 1 || mouse == 2) fill(img, w, h, s_mouse_button, body, GOLD);
		float ring[6] = {cx, cy, hx - line * 0.5f, hy - line * 0.5f, r, line * 0.5f}; fill(img, w, h, s_ring, ring, WHITE);
		float wheel[5] = {cx, cy - hy * 0.55f, line * 0.6f, hy * 0.18f, line * 0.6f}; fill(img, w, h, s_box, wheel, mouse == 3 ? GOLD : WHITE);
		float cut[5] = {cx, split, hx, line * 0.5f, 0}; fill(img, w, h, s_box, cut, WHITE);
		return 1;
	}
	if (!label || !*label) return 0;
	float hx = w * 0.5f - m, hy = h * 0.5f - m, r = (hx < hy ? hx : hy) * 0.28f;
	if (strlen(label) <= 1 && w > h * 1.2f) hx = hy;   /* (a letter's cap is square even in a wide glyph) */
	float edge[5] = {cx, cy + m * 0.5f, hx, hy - m * 0.5f, r}; fill(img, w, h, s_box, edge, EDGE);   /* the cap's side, showing below its face */
	float face[5] = {cx, cy - m * 0.3f, hx - m * 0.35f, hy - m * 0.8f, r * 0.9f}; fill(img, w, h, s_box, face, CAP);
	if ((unsigned char)label[0] < 5 && !label[1]) {   /* "\1".."\4": an arrow up, down, left, right */
		const float a = (hy < hx ? hy : hx) * 0.42f, y0 = cy - m * 0.3f; float t[6];
		switch (label[0]) {
		case 1: t[0] = cx; t[1] = y0 - a; t[2] = cx + a; t[3] = y0 + a * 0.6f; t[4] = cx - a; t[5] = y0 + a * 0.6f; break;
		case 2: t[0] = cx; t[1] = y0 + a; t[2] = cx - a; t[3] = y0 - a * 0.6f; t[4] = cx + a; t[5] = y0 - a * 0.6f; break;
		case 3: t[0] = cx - a; t[1] = y0; t[2] = cx + a * 0.6f; t[3] = y0 - a; t[4] = cx + a * 0.6f; t[5] = y0 + a; break;
		default: t[0] = cx + a; t[1] = y0; t[2] = cx - a * 0.6f; t[3] = y0 + a; t[4] = cx - a * 0.6f; t[5] = y0 - a; break;
		}
		fill(img, w, h, s_tri, t, INK);
		return 1;
	}
	text(img, w, h, label, 2 * (hx - m * 0.35f), 2 * (hy - m * 0.8f));
	return font_ready();
}
