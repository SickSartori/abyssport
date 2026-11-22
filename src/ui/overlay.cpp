/*
 * Adapted from Project Nativ (https://github.com/TheLostPantheon/nativ), runtime/ui/overlay.cpp at 4b1c428:
 * GNU General Public License, version 2 or (at your option) any later version.
 */
/*
 * The in-game overlay: the one C++ translation unit in this project, so Dear ImGui
 * (third_party/imgui/) stays out of every other file. overlay.h is the plain-C contract the rest of
 * the host (window.c, mainly) calls through; this file never reaches back into window.c's own
 * statics -- it calls the small getters/setters window.c, gfx_gpu.c, metrics.c and settings.c each
 * expose for it, declared locally below, the same way every other file in this project declares an
 * `extern` for a function it needs instead of sharing a header (see e.g. window.c's own use of
 * gfx_gpu_render_scale_cycle).
 *
 * Two states, one ImGui frame per present (window.c's present loop calls overlay_frame, then
 * overlay_marks; exactly one of them builds, keyed on overlay_is_open()):
 *   - the menu (build_ui; F1 or Guide in window.c): abyssport's launcher's pages over the paused game
 *     (src/launcher/launcher.cpp's in-game mode, launcher_game.h). The game is paused while it's up
 *     (display.c), every event is ImGui's (window.c), keyboard and gamepad navigation are on.
 *   - closed (build_closed): the HUD -- the performance overlay's lines and the gamepad-touch
 *     markers -- the toasts, a one-time hint toast (settings key menu_hint_shown) and the OS cursor
 *     hidden after a few seconds still over the picture. Nothing in it takes the game's keys or pad;
 *     window.c asks overlay_mouse_captured() before it turns a mouse event into a touch.
 *   - the shader prep card (build_prep): a start whose shader warm-up would take more than a few seconds (gfx_gpu.c's
 *     shader prep: a first start, an update's new recipes, a cleared OS cache) pauses the game under it with the
 *     progress and Skip; it closes by itself when the builds are done. Skipped (or closed any way: Escape, F1, the
 *     quit card's Keep playing), the builds go on behind the game with a line in the corner until they're done.
 *   - the touch card (build_intro): the first game a player starts shows it over the game, paused as the menu
 *     pauses it, so input keeps its in-game path (the hidden, captured mouse, Alt's touch pointer, a bumper's
 *     crosshair, the touchpad's marks): how to touch the Vita's screen with a keyboard and mouse, a controller or a
 *     touch screen, the device in use highlighted; done by a real touch on either panel (window_touch_contacts), or
 *     skipped with Escape or F1. Once per player: shared settings key touch_intro_shown.
 *   - the quit card (build_quit): a request to close the game window (Cmd+W, Cmd+Q, the close button, Alt+F4) opens it
 *     first, the game paused: Keep playing, or Quit; a second request while it's up quits (window.c).
 *   - headless, one-shot (overlay_render_forced, RECOMP_OVERLAY_DUMP: window.c's own comment) --
 *     never touches overlay_is_open()/open/close, so it can never leave the real pause/input/audio
 *     state the rest of the host keys off that open flag for stuck on. No platform backend (no
 *     window to attach one to): the caller's own w/h stand in for a display size, there is no
 *     pointer, and RECOMP_OVERLAY_DUMP_UI picks what to force on screen (see overlay.h).
 */
#include "imgui.h"
#include "imgui_internal.h"   /* ClosePopupsExceptModals (Escape with a bar menu open: overlay_bar_dismiss below); ImFontAtlasTextureBlockConvert (prompts_load) */
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include "gpulock.h"   /* the SDL submit below is the locked one, and ImGui's own submits run under the same lock (render_frame): Homebrew SDL 3.4.16's Metal fence race, see that header */
#include "overlay.h"
#include "savedlg.h"
#include "ui_theme.h"
#define STBI_NO_STDIO
#include "third_party/stb/stb_image.h"   /* (its implementation: runtime/media/jpeg.c) */
#include "prompts_png.h"                 /* third_party/kenney-prompts/prompts.png as bytes (runtime/build.py writes it) */
#include "game_texts.h"                  /* the cards' words, in the launcher's language */
#include "launcher_game.h"               /* the menu: abyssport's launcher (src/launcher/launcher.cpp) */
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

extern "C" {
void display_set_paused(int on);   /* display.c */
void window_set_fullscreen(int on);   /* window.c */
/* gfx_gpu.c: the GPU backend's internal resolution, and the shader prep card's state (build_prep) */
void gfx_gpu_render_scale_request(int n);
int gfx_gpu_shader_prep(int *done, int *total, double *eta_s); void gfx_gpu_shader_prep_skip(void);
/* metrics.c: the F10 overlay's level and its text, drawn here as the HUD (overlay_marks) */
void metrics_osd_set(int level);
int metrics_osd_lines(char (*out)[40], int max);
/* settings.c */
int settings_has(const char *key);
int settings_int(const char *key, int def);
void settings_set_int(const char *key, int v);
void settings_save(void);
}

static ImGuiContext *g_ctx;
static bool g_has_window;      /* overlay_init was given a real SDL_Window: the platform backend is live */
/* written by the window thread, read by the game thread (display.c's pause, window.c's audio): atomic,
 * like every other flag that crosses those threads (rscale_req, osd_level_v). g_paused: the touch card
 * holds the game (display.c, as for the menu) */
static std::atomic<bool> g_open, g_want_capture, g_paused;
static std::atomic<int> g_want_quit;   /* 1 quit (back to the launcher that started the game), 2 quit the launcher too */
static std::atomic<bool> g_intro;                 /* the touch card is up (the game paused under it) */
static void intro_end(void); static void build_intro(void);
static std::atomic<bool> g_prep_card;             /* the shader prep card is up (in the menu's open state: the game paused, input to the card) */
static int g_prep_demo[3] = {-1, 0, 0};           /* RECOMP_OVERLAY_DUMP_UI=prep: the card with these numbers (done, total, seconds left) */
static void build_prep(void); static void build_prep_corner(float w, float hgt, int done, int total);
static std::atomic<bool> g_quit_card;             /* the quit card is (overlay_quit_confirm) */
static std::atomic<int> g_layout_ask, g_layout_answer;   /* the layout card: asked, and its answer (1 touch, 2 PS TV) */
static bool g_layout_card, g_layout_remember;

static void apply_setting(const char *key);
int overlay_init(SDL_GPUDevice *device, SDL_Window *window, SDL_GPUTextureFormat color_fmt) {
	if (!device) return 0;
	if (g_ctx) return 1;   /* window.c calls this once at start-up; a repeat (shouldn't happen) is harmless */
	IMGUI_CHECKVERSION();
	g_ctx = ImGui::CreateContext();
	ImGui::SetCurrentContext(g_ctx);
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;   /* never write imgui.ini into the game's own run directory */
	/* (keyboard/gamepad navigation: set per frame in render_frame -- on with the full menu only) */
	ui::load_system_font(io);   /* an explicit face: without one, ImGui 1.92's own automatic choice (vector vs. bitmap)
		depends on a DPI scale the headless path never sets, and picks a degenerate (zero-size) one */
	ui::apply(15.0f, 1.0f);     /* the cards' and toasts' look (ui_theme.h) */
	launcher_game_init(device, apply_setting);   /* the menu: the launcher's pages, its letters into this atlas */
	g_has_window = window != nullptr;
	if (g_has_window && !ImGui_ImplSDL3_InitForSDLGPU(window)) { ImGui::DestroyContext(g_ctx); g_ctx = nullptr; return 0; }
	ImGui_ImplSDLGPU3_InitInfo info = {};
	info.Device = device; info.ColorTargetFormat = color_fmt; info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
	if (!ImGui_ImplSDLGPU3_Init(&info)) {
		if (g_has_window) ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext(g_ctx); g_ctx = nullptr;
		return 0;
	}
	return 1;
}
void overlay_shutdown(void) {
	if (!g_ctx) return;
	ImGui::SetCurrentContext(g_ctx);
	ImGui_ImplSDLGPU3_Shutdown();
	if (g_has_window) ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext(g_ctx);
	g_ctx = nullptr; g_has_window = false; g_open = false; g_want_quit = 0; g_want_capture = false; g_paused = false;
}

int overlay_is_open(void) { return g_ctx && g_open; }
/* every pause goes through here: display.c freezes the game's clock for exactly the time the menu, a card
 * or the touch card is up (overlay_game_paused is what display.c and window.c's
 * audio actually poll) */
static void apply_pause(void) { display_set_paused(g_open || g_paused); }
void overlay_open(void) { if (g_ctx) { intro_end(); if (!g_open) launcher_game_open(); g_open = true; apply_pause(); } }   /* (F1 over the touch card: it's skipped) */
static void intro_end(void) {   /* the touch card, done or skipped: never again */
	if (!g_intro) return;
	g_intro = false; settings_set_int("touch_intro_shown", 1); settings_save();
	g_paused = false; apply_pause();
}
void overlay_close(void) {   /* closing the menu resumes */
	if (g_layout_card) { g_layout_card = false; g_layout_ask = 0; g_layout_answer = 1; }   /* (Escape on the layout card: touch, the default) */
	intro_end();
	if (g_prep_card) { g_prep_card = false; gfx_gpu_shader_prep_skip(); }   /* (the card closed before the builds were done: Skip) */
	g_quit_card = false;
	g_open = false; g_paused = false; apply_pause();
}
void overlay_toggle(void) { if (g_open) overlay_close(); else overlay_open(); }
void overlay_escape(void) { if (g_quit_card || g_layout_card || g_prep_card) overlay_close(); }
int overlay_game_paused(void) { return g_ctx && (g_open || g_paused); }
int overlay_wants_quit(void) { return g_want_quit.exchange(0); }
int overlay_quit_confirm(void) {   /* (window.c, on a request to close the window) 1: the card is up now; 0: it already was, or can't be: quit */
	if (!g_ctx || !g_has_window || g_quit_card) return 0;
	g_quit_card = true; overlay_open(); return 1;
}
/* the cards drawn here over the game (the save-data dialog, the quit card): the text a whole multiple of its size for
 * every 540 lines of the window (the Vita's own height, 544) -- the base size alone was a speck on a large screen */
static float card_scale(float height) { return fmaxf(1.0f, floorf(height / 540.0f)); }
int overlay_wants_capture(void) { return g_want_capture.exchange(false); }

extern "C" int window_kbm_active(void);   /* window.c: keyboard and mouse were used last */
void overlay_process_event(const SDL_Event *event) {
	if (!g_ctx || !g_has_window) return;
	ImGui::SetCurrentContext(g_ctx);
	if (g_open && launcher_game_event(event)) return;   /* (the key for an action being changed) */
	ImGui_ImplSDL3_ProcessEvent(event);
}

/* ---- the menu (F1): abyssport's launcher, its pages over the paused game (src/launcher/launcher.cpp, launcher_game.h) */
/* a setting the launcher's pages changed that the game takes at once only when told; the others it reads as it goes,
 * and the keys when the menu closes (window.c's kb_reload) */
static void apply_setting(const char *key) {
	if (!std::strcmp(key, "render_scale")) gfx_gpu_render_scale_request(settings_int("render_scale", 1));
	else if (!std::strcmp(key, "fullscreen")) window_set_fullscreen(settings_int("fullscreen", 0));
	else if (!std::strcmp(key, "osd")) metrics_osd_set(settings_int("osd", 0));
}
static void build_ui(void) {
	const ImGuiIO &io = ImGui::GetIO();
	const int r = launcher_game_frame((int)io.DisplaySize.x, (int)io.DisplaySize.y);
	if (r == 1) { std::fprintf(stderr, "    menu: closed (Resume)\n"); overlay_close(); }
	else if (r == 2) g_want_quit = 1;
	else if (r == 3) g_want_quit = 2;
}

/* ---- everything the closed state shows (build_closed). All of it runs on the window thread (window.c's present
 * loop), like the menu. Times are ImGui's own clock (GetTime: io.DeltaTime summed), so the headless path's fixed
 * 1/60 s steps work the same way. */
static const float TOAST_S = 4.5f, CURSOR_HIDE_S = 3.0f;
static bool g_toast_checked;   /* the first-launch menu hint was looked at */
/* the toasts: badges at the top right, newest on top (overlay_toast_titled: posted by any thread, taken in by the next frame) */
struct Toast { char title[48]; char body[224]; double start, until; };
static Toast g_toasts[3]; static int g_ntoasts;
static Toast g_toast_q[8]; static int g_toast_qn; static std::mutex g_toast_mx;
static double g_idle_since; static ImVec2 g_last_mouse(-1, -1);
static bool g_mouse_captured;           /* io.WantCaptureMouse as of this thread's last ImGui frame (overlay_mouse_captured) */
static bool g_force_toast;              /* RECOMP_OVERLAY_DUMP_UI=toast (overlay_render_forced) */
static bool g_touch_cursor;             /* overlay_touch_cursor: the pointer is a touch mark this frame -- no arrow */
static bool g_skip_draw;                /* overlay_render_forced's warm-up frames: built (ImGui sizes its windows) but not drawn */

void overlay_bar_dismiss(void) { if (g_ctx && g_intro) intro_end(); }   /* (Escape with the menu closed: the touch card skipped) */
int overlay_mouse_captured(void) { return g_ctx && g_has_window && g_mouse_captured; }

extern "C" void overlay_touch_cursor(int on) { g_touch_cursor = on != 0; }
extern "C" void overlay_toast_titled(const char *title, const char *body, double seconds) {
	std::lock_guard<std::mutex> l(g_toast_mx);
	if (g_toast_qn == 8) { std::memmove(g_toast_q, g_toast_q + 1, sizeof g_toast_q[0] * 7); g_toast_qn--; }   /* (a burst: the oldest unseen goes) */
	Toast &q = g_toast_q[g_toast_qn++];
	std::snprintf(q.title, sizeof q.title, "%s", title ? title : ""); std::snprintf(q.body, sizeof q.body, "%s", body ? body : "");
	q.start = 0; q.until = seconds;   /* (a duration until taken in) */
}
extern "C" void overlay_toast(const char *msg, double seconds) { overlay_toast_titled(nullptr, msg, seconds); }
/* the posted toasts into the stack: one already showing (same title and text) runs on instead of stacking twice;
 * three at most, the oldest dropped; each gone when its time is up */
static void toasts_take(double t) {
	{
		std::lock_guard<std::mutex> l(g_toast_mx);
		for (int i = 0; i < g_toast_qn; i++) {
			Toast q = g_toast_q[i]; double dur = q.until;
			int k = 0; while (k < g_ntoasts && (std::strcmp(g_toasts[k].title, q.title) || std::strcmp(g_toasts[k].body, q.body))) k++;
			if (k < g_ntoasts) { g_toasts[k].until = t + dur; continue; }
			if (g_ntoasts == 3) g_ntoasts--;
			std::memmove(g_toasts + 1, g_toasts, sizeof g_toasts[0] * g_ntoasts); g_ntoasts++;
			q.start = t; q.until = t + dur; g_toasts[0] = q;
		}
		g_toast_qn = 0;
	}
	int n = 0; for (int i = 0; i < g_ntoasts; i++) if (t < g_toasts[i].until) g_toasts[n++] = g_toasts[i];
	g_ntoasts = n;
}
/* A toast as a badge at the top right: a dark card with a gold edge, the
 * title in amber, the text under it, wrapped to a third of the window; "[F1]" in the text draws as a key cap.
 * It slides in, and fades over its last 0.4 s. */
struct ToastTok { const char *s; int n; bool key, brk; float w; };
static int toast_tokens(const char *body, ImFont *font, float fs, float cap_pad, ToastTok *out, int max) {
	int n = 0;
	for (const char *p = body; *p && n < max; ) {
		if (*p == '\n') { out[n++] = {p, 0, false, true, 0}; p++; continue; }
		if (*p == ' ') { p++; continue; }
		const char *e = p; bool key = *p == '[' && std::strchr(p, ']');
		if (key) { e = std::strchr(p, ']') + 1; out[n] = {p + 1, (int)(e - p - 2), true, false, 0}; }
		else { while (*e && *e != ' ' && *e != '\n' && !(*e == '[' && e > p)) e++; out[n] = {p, (int)(e - p), false, false, 0}; }
		out[n].w = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, out[n].s, out[n].s + out[n].n).x + (key ? 2 * cap_pad : 0);
		n++; p = e;
	}
	return n;
}
static void build_toasts(float w, float hgt, double t) {
	(void)hgt;
	ImFont *font = ImGui::GetFont(); const float fs = ImGui::GetFontSize() * (w >= 1000 ? 2.0f : 1.0f);   /* (the bitmap font: integer multiples stay crisp) */
	const float pad = fs * 0.75f, edge = fs * 0.3f, cap_pad = fs * 0.3f, space = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, " ").x, lh = fs * 1.35f;
	const float maxw = fmaxf(w / 3.0f, fs * 18.0f);
	float y = fs * 0.75f;
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	for (int i = 0; i < g_ntoasts; i++) {
		const Toast &q = g_toasts[i];
		const float in = g_has_window ? (float)fmin(1.0, (t - q.start) / 0.15) : 1.0f, a = (float)fmin(1.0, (q.until - t) / 0.4) * in;   /* (a headless dump: as it settles) */
		ToastTok tok[64]; const int nt = toast_tokens(q.body, font, fs, cap_pad, tok, 64);
		/* the lines: tokens laid left to right, a new line at a "\n" or where the next would pass maxw */
		float lx = 0, widest = q.title[0] ? font->CalcTextSizeA(fs, FLT_MAX, 0.0f, q.title).x : 0; int lines = nt ? 1 : 0;
		float tx[64], ty_line[64];
		for (int k = 0; k < nt; k++) {
			if (tok[k].brk) { lines++; lx = 0; tx[k] = 0; ty_line[k] = (float)(lines - 1); continue; }
			if (lx > 0 && lx + space + tok[k].w > maxw) { lines++; lx = 0; }
			if (lx > 0) lx += space;
			tx[k] = lx; ty_line[k] = (float)(lines - 1); lx += tok[k].w; if (lx > widest) widest = lx;
		}
		const float cw = widest + 2 * pad + edge, ch = (q.title[0] ? lh : 0) + lines * lh + 2 * pad - (lh - fs);
		const float x0 = w - fs * 0.75f - cw + (1 - in) * fs * 1.5f;
		const ImVec2 p0(x0, y), p1(x0 + cw, y + ch);
		dl->AddRectFilled(p0, p1, IM_COL32(14, 12, 10, (int)(a * 235)));   /* (the launcher's panel, square) */
		dl->AddRectFilled(p0, ImVec2(p0.x + edge, p1.y), IM_COL32(226, 168, 44, (int)(a * 255)));
		dl->AddRect(p0, p1, IM_COL32(255, 255, 255, (int)(a * 26)));
		float cy = y + pad, cx = x0 + edge + pad;
		if (q.title[0]) { dl->AddText(font, fs, ImVec2(cx, cy), IM_COL32(226, 168, 44, (int)(a * 255)), q.title); cy += lh; }
		for (int k = 0; k < nt; k++) {
			if (tok[k].brk) continue;
			const ImVec2 at(cx + tx[k], cy + ty_line[k] * lh);
			if (tok[k].key) {   /* a key cap */
				dl->AddRectFilled(ImVec2(at.x, at.y - fs * 0.12f), ImVec2(at.x + tok[k].w, at.y + fs * 1.12f), IM_COL32(232, 232, 236, (int)(a * 255)), fs * 0.2f);   /* (a key, as the launcher's bar draws them) */
				dl->AddRect(ImVec2(at.x, at.y - fs * 0.12f), ImVec2(at.x + tok[k].w, at.y + fs * 1.12f), IM_COL32(150, 156, 172, (int)(a * 255)), fs * 0.2f);
				dl->AddText(font, fs, ImVec2(at.x + cap_pad, at.y), IM_COL32(6, 6, 7, (int)(a * 255)), tok[k].s, tok[k].s + tok[k].n);
			} else dl->AddText(font, fs, at, IM_COL32(232, 234, 240, (int)(a * 255)), tok[k].s, tok[k].s + tok[k].n);
		}
		y = p1.y + fs * 0.5f;
	}
}
/* the HUD while the menu is closed: the touch markers (window.c's gamepad touch) and the performance overlay
 * (metrics.c's F10 lines) -- no window, no input, the foreground draw list only */
static const OverlayMark *g_marks; static int g_nmarks;
static float g_rear[4];   /* the rear panel's rows on the picture, in window points (x, y, w, h); w 0: none */
static float g_focus[4];  /* the game menu's button the pad is on (window.c, menunav.h), window points; w 0: none */
void overlay_menu_focus(const float rect[4]) { for (int i = 0; i < 4; i++) g_focus[i] = rect ? rect[i] : 0; }
static char g_osd[6][40]; static int g_nosd;   /* the overlay's lines this frame (metrics_osd_lines) */
int overlay_draws_hud(void) { return g_ctx && g_has_window; }
static void build_marks(void) {
	ImDrawList *dl = g_intro ? ImGui::GetForegroundDrawList() : ImGui::GetBackgroundDrawList();   /* over the game's frame (LOAD); over the touch card */
	if (g_nosd) {   /* the overlay: a translucent dark panel at the top-left corner with the lines, as the bitmap version drew it (metrics.c) */
		ImFont *font = ImGui::GetFont(); float fs = ImGui::GetFontSize() * 2.0f;   /* the embedded bitmap font at an integer multiple: crisp */
		const float x = 8, y = 8, pad = 6, lh = fs + 2; float wmax = 0;
		for (int i = 0; i < g_nosd; i++) { ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, g_osd[i]); if (sz.x > wmax) wmax = sz.x; }
		dl->AddRectFilled(ImVec2(x, y), ImVec2(x + wmax + 2 * pad, y + g_nosd * lh + 2 * pad), IM_COL32(0, 0, 0, 170));
		for (int i = 0; i < g_nosd; i++) dl->AddText(font, fs, ImVec2(x + pad, y + pad + i * lh), IM_COL32(255, 255, 255, 255), g_osd[i]);
	}
	if (g_focus[2] > 0) {   /* the pad's place in the game's menu: a short thin line under the button, its middle half */
		const float k = ImGui::GetMainViewport()->Size.y / 544.0f, h = fmaxf(2.0f, 1.5f * k), y = g_focus[1] + g_focus[3] + 3 * k;
		const float cx = g_focus[0] + g_focus[2] * 0.5f, half = g_focus[2] * 0.25f;
		dl->AddRectFilled(ImVec2(cx - half, y), ImVec2(cx + half, y + h), IM_COL32(255, 255, 255, 170), h * 0.5f);
	}
	if (g_rear[2] > 0) dl->AddRect(ImVec2(g_rear[0], g_rear[1]), ImVec2(g_rear[0] + g_rear[2], g_rear[1] + g_rear[3]), IM_COL32(255, 170, 60, 140), 0.0f, 2.0f);   /* (rounding 0, thickness 2: ImGui 1.92.8+ argument order -- the old (0, 0, 2.0f) put 2 in `flags`, an ImGui user-error assert) */
	for (int i = 0; i < g_nmarks; i++) {
		const OverlayMark &m = g_marks[i];
		ImU32 col = m.rear ? IM_COL32(255, 170, 60, 230) : IM_COL32(80, 200, 255, 230);   /* rear orange, front blue */
		float r = m.cursor ? 14.f : 10.f;
		if (m.pressed) dl->AddCircleFilled(ImVec2(m.x, m.y), r, (col & 0x00ffffff) | IM_COL32(0, 0, 0, 120));
		dl->AddCircle(ImVec2(m.x, m.y), r, col, 0, 2.5f);
		if (m.cursor) { dl->AddLine(ImVec2(m.x - r - 4, m.y), ImVec2(m.x + r + 4, m.y), col, 1.5f); dl->AddLine(ImVec2(m.x, m.y - r - 4), ImVec2(m.x, m.y + r + 4), col, 1.5f); }
	}
}
/* the save-data dialog (apputil.c through savedlg.h): the game's save / load / delete list, a yes/no question,
 * or a card the game closes itself (Saving..., a progress bar) -- over the picture while the game waits on it,
 * the game running underneath (it polls the dialog every frame; nothing pauses). The pad: up/down (left/right
 * on a question), Cross to choose, Circle to go back; the keyboard: arrows, Enter or Space, Escape or
 * Backspace; the mouse: hover and click. window.c hands this the keys and the pad while overlay_dialog_active
 * (ImGui's SDL backend polls the pad itself, so its buttons arrive whatever the routing). */
static int g_sd_seq = -1, g_sd_sel; static bool g_sd_armed;
int overlay_dialog_active(void) {
	if (!g_ctx || !g_has_window) return 0;
	SaveDlgView v; const int k = savedlg_view(&v); savedlg_release();
	return k == SAVEDLG_LIST || k == SAVEDLG_YESNO;
}
static const ImGuiKey K_UP[] = {ImGuiKey_UpArrow, ImGuiKey_W, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp};
static const ImGuiKey K_DOWN[] = {ImGuiKey_DownArrow, ImGuiKey_S, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadLStickDown};
static const ImGuiKey K_SIDE[] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_A, ImGuiKey_D, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight};
static const ImGuiKey K_OK[] = {ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_Space, ImGuiKey_GamepadFaceDown};
static const ImGuiKey K_BACK[] = {ImGuiKey_Escape, ImGuiKey_Backspace, ImGuiKey_GamepadFaceRight};
template <size_t N> static bool keys_pressed(const ImGuiKey (&k)[N]) { for (ImGuiKey x : k) if (ImGui::IsKeyPressed(x, true)) return true; return false; }
template <size_t N> static bool keys_down(const ImGuiKey (&k)[N]) { for (ImGuiKey x : k) if (ImGui::IsKeyDown(x)) return true; return false; }
static void build_save_dialog(float w, float hgt) {
	SaveDlgView v; const int kind = savedlg_view(&v);
	if (kind == SAVEDLG_NONE) { savedlg_release(); g_sd_seq = -1; return; }
	if (v.seq != g_sd_seq) { g_sd_seq = v.seq; g_sd_sel = kind == SAVEDLG_LIST ? v.focus : 0; g_sd_armed = false; }
	/* a key already down as the step appeared (the press that asked the game to save) answers nothing */
	if (!g_sd_armed && !keys_down(K_OK) && !keys_down(K_BACK)) g_sd_armed = true;
	const bool ok = g_sd_armed && keys_pressed(K_OK), back = g_sd_armed && keys_pressed(K_BACK);
	int answer = 0; bool answered = false, moved = false;
	const float s = card_scale(hgt);
	ImGui::SetNextWindowPos(ImVec2(w * 0.5f, hgt * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(fminf(w - 32, 520 * s), 0), ImGuiCond_Always);   /* (height 0: fitted every frame) */
	ImGui::SetNextWindowBgAlpha(0.95f);
	ImGui::Begin("##savedlg", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * s);
	const ImU32 dim = ImGui::GetColorU32(ImGuiCol_TextDisabled), txt = ImGui::GetColorU32(ImGuiCol_Text);
	if (kind == SAVEDLG_LIST) {
		ImGui::TextUnformatted(v.title[0] ? v.title : game_text(GT_SAVED_DATA));
		ImGui::Separator();
		if (g_sd_armed && v.nrows) {
			if (keys_pressed(K_UP) && g_sd_sel > 0) { g_sd_sel--; moved = true; }
			if (keys_pressed(K_DOWN) && g_sd_sel < v.nrows - 1) { g_sd_sel++; moved = true; }
		}
		if (g_sd_sel >= v.nrows) g_sd_sel = v.nrows ? v.nrows - 1 : 0;
		if (!v.nrows) ImGui::TextDisabled("%s", game_text(GT_NO_DATA));
		else {
			const float lh = ImGui::GetTextLineHeight(), rowh = 3 * lh + 12 * s;
			ImGui::BeginChild("rows", ImVec2(0, (rowh + ImGui::GetStyle().ItemSpacing.y) * (float)(v.nrows < 5 ? v.nrows : 5)));
			ImFont *font = ImGui::GetFont(); const float fs = ImGui::GetFontSize();
			for (int i = 0; i < v.nrows; i++) {
				const SaveDlgRow &r = v.rows[i];
				ImGui::PushID(i);
				const ImVec2 p = ImGui::GetCursorScreenPos(); const float rw = ImGui::GetContentRegionAvail().x;
				if (ImGui::Selectable("##row", i == g_sd_sel, 0, ImVec2(0, rowh))) { answer = i; answered = true; }
				if (ImGui::IsItemHovered() && (ImGui::GetIO().MouseDelta.x || ImGui::GetIO().MouseDelta.y)) g_sd_sel = i;
				if (moved && i == g_sd_sel) ImGui::SetScrollHereY(0.5f);
				ImDrawList *dl = ImGui::GetWindowDrawList();
				dl->AddText(ImVec2(p.x + 6 * s, p.y + 4 * s), txt, r.title);
				if (r.exists) {
					const float dw = ImGui::CalcTextSize(r.date).x;
					dl->AddText(ImVec2(p.x + rw - dw - 6 * s, p.y + 4 * s), dim, r.date);
					if (r.sub[0]) dl->AddText(ImVec2(p.x + 6 * s, p.y + 6 * s + lh), txt, r.sub);
					if (r.detail[0]) dl->AddText(ImVec2(p.x + 6 * s, p.y + 8 * s + 2 * lh), dim, r.detail);
				} else dl->AddText(ImVec2(p.x + 6 * s, p.y + 6 * s + lh), dim, game_text(GT_EMPTY));
				ImGui::PopID();
			}
			ImGui::EndChild();
		}
		if (ok && v.nrows) { answer = g_sd_sel; answered = true; }
		ImGui::Separator();
		if (ImGui::Button(game_text(GT_BACK)) || back) { answer = SAVEDLG_CANCEL; answered = true; }
		ImGui::SameLine(); ImGui::TextDisabled("  %s", game_text(GT_LIST_HINT));
	} else if (kind == SAVEDLG_YESNO) {
		ImGui::PushTextWrapPos(0); ImGui::TextUnformatted(v.msg[0] ? v.msg : game_text(GT_CONTINUE)); ImGui::PopTextWrapPos();
		ImGui::Spacing();
		if (g_sd_armed && (keys_pressed(K_SIDE) || keys_pressed(K_UP) || keys_pressed(K_DOWN))) g_sd_sel ^= 1;
		const char *const names[2] = {game_text(GT_YES), game_text(GT_NO)};
		for (int b = 0; b < 2; b++) {
			if (b) ImGui::SameLine();
			const bool sel = g_sd_sel == b;
			if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			if (ImGui::Button(names[b], ImVec2(90 * s, 0))) { answer = b ? SAVEDLG_NO : SAVEDLG_YES; answered = true; }
			if (sel) ImGui::PopStyleColor();
			if (ImGui::IsItemHovered() && (ImGui::GetIO().MouseDelta.x || ImGui::GetIO().MouseDelta.y)) g_sd_sel = b;
		}
		if (ok) { answer = g_sd_sel ? SAVEDLG_NO : SAVEDLG_YES; answered = true; }
		if (back) { answer = SAVEDLG_CANCEL; answered = true; }
		ImGui::SameLine(); ImGui::TextDisabled("  %s", game_text(GT_YESNO_HINT));
	} else {   /* SAVEDLG_WAIT: the game closes it */
		ImGui::PushTextWrapPos(0); ImGui::TextUnformatted(v.msg[0] ? v.msg : game_text(GT_WAIT)); ImGui::PopTextWrapPos();
		if (v.percent >= 0) ImGui::ProgressBar(v.percent / 100.0f, ImVec2(-1, 0));
	}
	ImGui::PopFont();
	ImGui::End();
	const int seq = v.seq;
	savedlg_release();   /* (before answering: savedlg_answer takes the same lock) */
	if (answered) savedlg_answer(seq, answer);
}
/* everything the closed state shows (see the file's header comment) */
static void build_closed(void) {
	ImGuiIO &io = ImGui::GetIO();
	const float w = io.DisplaySize.x, hgt = io.DisplaySize.y;
	const double t = ImGui::GetTime();
	const bool popup_open = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	if (g_has_window) {
		if (!g_toast_checked) {   /* the hint, once per game, on this overlay's first windowed frame */
			g_toast_checked = true;
			if (!settings_has("touch_intro_shown")) {   /* the player's first game: the touch card first (it says where the menu is too) */
				g_intro = true; settings_set_int("menu_hint_shown", 1); settings_save(); g_paused = true; apply_pause();
			} else if (!settings_has("menu_hint_shown")) { overlay_toast_titled(game_text(GT_T_MENU), game_text(GT_MENU_HINT), TOAST_S); settings_set_int("menu_hint_shown", 1); settings_save(); }
		}
		/* the OS cursor: hidden after a few seconds still over the picture, back the moment it moves */
		const ImVec2 m = io.MousePos;
		const bool moved = m.x != g_last_mouse.x || m.y != g_last_mouse.y; g_last_mouse = m;
		if (moved || ImGui::IsAnyMouseDown()) g_idle_since = t;
		if (!popup_open && !io.WantCaptureMouse && !g_paused && t - g_idle_since > CURSOR_HIDE_S) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		if (g_touch_cursor && !popup_open && !io.WantCaptureMouse) ImGui::SetMouseCursor(ImGuiMouseCursor_None);   /* (the touch mark stands in for it) */
	} else if (g_force_toast) {   /* headless (the dump): RECOMP_OVERLAY_DUMP_UI=toast */
		g_force_toast = false;
		overlay_toast_titled(game_text(GT_T_MENU), game_text(GT_MENU_HINT), TOAST_S);
		char bal[160]; snprintf(bal, sizeof bal, game_text(GT_BAL_KEYS), "A", "D"); overlay_toast_titled(game_text(GT_T_BALANCE), bal, TOAST_S);
	}
	if (g_has_window) {   /* the shader prep (gfx_gpu.c): the card when it asks (after the touch card), the corner line while skipped, a toast when done */
		int done, total; double eta; const int ps = gfx_gpu_shader_prep(&done, &total, &eta);
		if (g_layout_ask && !g_layout_card && !g_intro && !g_open) { g_layout_card = true; g_open = true; apply_pause(); }   /* (the game waits for it: before the prep card) */
		else if (ps == 1 && !g_intro && !g_open && !g_layout_card) { g_prep_card = true; g_open = true; apply_pause(); }
		else if (ps == 2 && total > 0) build_prep_corner(w, hgt, done, total);
		else if (ps == 3) overlay_toast_titled(game_text(GT_T_SHADERS_READY), game_text(GT_SHADERS_READY), 4.0);
	}
	if (g_intro) build_intro();   /* (the touch card in place of the pause banner; the markers drawn over it) */
	build_marks();
	if (g_has_window) build_save_dialog(w, hgt);
	toasts_take(t);
	if (g_ntoasts) build_toasts(w, hgt, t);
}

/* shared by overlay_frame, overlay_marks and overlay_render_forced: builds one ImGui frame and renders
 * it into `target` with LOAD (never CLEAR -- the game's own frame, or the dump path's copy of it, is
 * already there and must show through everywhere the overlay doesn't draw). `have_window` picks
 * whether the SDL3 platform backend supplies DisplaySize/DeltaTime/mouse (interactive) or this
 * function sets them itself from the caller's w/h/dt_s (headless: see this file's own header
 * comment on the render paths). A frame that drew nothing (no HUD, no toast) costs no render pass. */
static void render_frame(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, uint32_t w, uint32_t h, double dt_s, bool have_window, void (*build)(void) = build_ui) {
	if (!g_ctx || !w || !h) return;
	ImGui::SetCurrentContext(g_ctx);
	ImGuiIO &io = ImGui::GetIO();
	/* keyboard and gamepad navigation only while the menu is up: while it's closed a game's keys and pad
	 * are the game's (window.c forwards only mouse events then) */
	if (build != build_ui) {   /* (the menu, the launcher's pages, sets its own as it goes: configure_navigation) */
		io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
		if (g_open) io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
	}
	ImGui_ImplSDLGPU3_NewFrame();
	if (have_window) {
		ImGui_ImplSDL3_NewFrame();
	} else {
		io.DisplaySize = ImVec2((float)w, (float)h);
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
		io.DeltaTime = dt_s > 0 ? (float)dt_s : 1.0f / 60.0f;
	}
	ImGui::NewFrame();
	g_mouse_captured = io.WantCaptureMouse;   /* (computed by NewFrame from where the pointer was: over a card or the save dialog) */
	build();
	ImGui::Render();
	ImDrawData *dd = ImGui::GetDrawData();
	if (g_skip_draw || dd->DisplaySize.x <= 0 || dd->DisplaySize.y <= 0 || dd->TotalVtxCount == 0) return;
	/* RECOMP_OVERLAY_DEBUG=1: what ImGui handed the backend this frame -- per draw command, the vertices it
	 * indexes: their position box, how many are not finite, the first one's colour (a verification aid
	 * for the headless dump, where the picture can't be watched being drawn) */
	static int dbg = -1; if (dbg < 0) dbg = std::getenv("RECOMP_OVERLAY_DEBUG") != nullptr;
	if (dbg) {
		std::fprintf(stderr, "    overlay frame: %d lists, %d vtx, %d idx\n", dd->CmdListsCount, dd->TotalVtxCount, dd->TotalIdxCount);
		for (int l = 0; l < dd->CmdListsCount; l++) {
			const ImDrawList *dl = dd->CmdLists[l];
			for (int c = 0; c < dl->CmdBuffer.Size; c++) {
				const ImDrawCmd &pc = dl->CmdBuffer[c];
				if (!pc.ElemCount) continue;
				float x0 = FLT_MAX, y0 = FLT_MAX, x1 = -FLT_MAX, y1 = -FLT_MAX; int bad = 0;
				for (unsigned i = 0; i < pc.ElemCount; i++) {
					const ImDrawVert &v = dl->VtxBuffer[dl->IdxBuffer[pc.IdxOffset + i] + pc.VtxOffset];
					if (!std::isfinite(v.pos.x) || !std::isfinite(v.pos.y) || !std::isfinite(v.uv.x) || !std::isfinite(v.uv.y)) { bad++; continue; }
					x0 = fminf(x0, v.pos.x); y0 = fminf(y0, v.pos.y); x1 = fmaxf(x1, v.pos.x); y1 = fmaxf(y1, v.pos.y);
				}
				const ImDrawVert &v0 = dl->VtxBuffer[dl->IdxBuffer[pc.IdxOffset] + pc.VtxOffset];
				std::fprintf(stderr, "      list %d '%s' cmd %d: %u idx (vtx off %u, idx off %u) clip (%.0f,%.0f)-(%.0f,%.0f) pos box (%.1f,%.1f)-(%.1f,%.1f) nonfinite %d first col %08x uv (%.4f,%.4f) texdata %p (status %d, %dx%d)\n",
					l, dl->_OwnerName ? dl->_OwnerName : "?", c, pc.ElemCount, pc.VtxOffset, pc.IdxOffset, pc.ClipRect.x, pc.ClipRect.y, pc.ClipRect.z, pc.ClipRect.w, x0, y0, x1, y1, bad, v0.col, v0.uv.x, v0.uv.y,
					(void *)pc.TexRef._TexData, pc.TexRef._TexData ? (int)pc.TexRef._TexData->Status : -1, pc.TexRef._TexData ? pc.TexRef._TexData->Width : 0, pc.TexRef._TexData ? pc.TexRef._TexData->Height : 0);
			}
		}
	}
	/* mandatory before the render pass below: uploads the vertex/index buffers. It also acquires and
	 * submits a command buffer of its own whenever ImGui has a texture to create or update (the font
	 * atlas: the first frame, and any frame a new glyph is rasterised), with SDL's plain
	 * SDL_SubmitGPUCommandBuffer -- the backend is third_party code built without gpulock.h's renames
	 * -- so the whole call runs under the host's submit lock, as every submit must (gpulock.h) */
	gpu_lock(); ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd); gpu_unlock();
	SDL_GPUColorTargetInfo ti = {};
	ti.texture = target; ti.load_op = SDL_GPU_LOADOP_LOAD; ti.store_op = SDL_GPU_STOREOP_STORE; ti.cycle = false;
	SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cmd, &ti, 1, nullptr);
	ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, rp);
	SDL_EndGPURenderPass(rp);
}
/* ---- the touch card (this file's header): the three ways to touch, the device last used highlighted */
enum { DEV_KBM, DEV_PAD, DEV_TOUCH };
static int g_intro_dev = -1; static double g_intro_done = -1;   /* the device highlighted (-1: not yet chosen); when the touch was made */
static double g_intro_at = -1;   /* headless (RECOMP_OVERLAY_DUMP_UI=intro@<seconds>): the moment of the loops drawn */
extern "C" int window_touch_contacts(int port, int *keys, int *xs, int *ys, int max);
/* the card's input icons: Kenney's Input Prompts (CC0), third_party/kenney-prompts/make.py's ICONS in this order, white
 * masks the card tints. Each pad family has seven from P_PAD: the left and right bumpers held and up, the south button
 * pressed and up, the left stick */
enum { P_OPTION, P_OPTION_UP, P_ALT, P_ALT_UP, P_MOUSE, P_MOUSE_LEFT, P_MOUSE_RIGHT, P_FINGER, P_PAD, P_N = P_PAD + 3 * 7 };
enum { PAD_L, PAD_L_UP, PAD_R, PAD_R_UP, PAD_A, PAD_A_UP, PAD_STICK };
enum { FAM_PS, FAM_XBOX, FAM_SWITCH };
static const int P_COLS = 8;
static ImFontAtlasRectId g_prompts[3] = {ImFontAtlasRectId_Invalid, ImFontAtlasRectId_Invalid, ImFontAtlasRectId_Invalid};   /* 128, 64 and 32 pixels a cell */
static void prompts_load(void) {   /* once, in a frame: the masks into ImGui's own font texture, which every path already draws from */
	static bool tried; if (tried) return; tried = true;
	int w, h, n; unsigned char *px = stbi_load_from_memory(prompts_png, (int)sizeof prompts_png, &w, &h, &n, 1);
	if (!px) return;
	ImFontAtlas *atlas = ImGui::GetIO().Fonts;
	for (int l = 0; l < 3; l++) {
		if (l) {   /* each level half the one before, a pixel the mean of four (in place: a write never passes a read) */
			for (int y = 0; y < h / 2; y++) for (int x = 0; x < w / 2; x++) { const unsigned char *s = px + 2 * y * w + 2 * x; px[y * (w / 2) + x] = (unsigned char)((s[0] + s[1] + s[w] + s[w + 1] + 2) / 4); }
			w /= 2; h /= 2;
		}
		ImFontAtlasRect r; g_prompts[l] = atlas->AddCustomRect(w, h, &r);   /* (queues its upload: the pixels are written before the frame renders) */
		if (g_prompts[l] == ImFontAtlasRectId_Invalid) break;
		ImTextureData *tex = atlas->TexData;   /* (after the add: it may have grown the texture) */
		ImFontAtlasTextureBlockConvert(px, ImTextureFormat_Alpha8, w, (unsigned char *)tex->GetPixelsAt(r.x, r.y), tex->Format, tex->GetPitch(), w, h);
	}
	stbi_image_free(px);
}
static void prompt(ImDrawList *dl, int k, ImVec2 c, float size, ImU32 col) {   /* icon k, size across, centred on c */
	const float fb = ImGui::GetIO().DisplayFramebufferScale.y, px = size * fb;
	int l = px > 70 ? 0 : px > 35 ? 1 : 2;   /* the smallest level about as big as the size drawn: drawn down, sharp, never up */
	while (l > 0 && g_prompts[l] == ImFontAtlasRectId_Invalid) l--;
	ImFontAtlas *atlas = ImGui::GetIO().Fonts; ImFontAtlasRect r;
	if (g_prompts[l] == ImFontAtlasRectId_Invalid || !atlas->GetCustomRect(g_prompts[l], &r)) return;
	const int rows = (P_N + P_COLS - 1) / P_COLS;
	const ImVec2 d((r.uv1.x - r.uv0.x) / P_COLS, (r.uv1.y - r.uv0.y) / rows), uv(r.uv0.x + k % P_COLS * d.x, r.uv0.y + k / P_COLS * d.y);
	const ImVec2 p(roundf((c.x - size * 0.5f) * fb) / fb, roundf((c.y - size * 0.5f) * fb) / fb);   /* (on whole pixels: sharp) */
	dl->AddImage(atlas->TexRef, p, ImVec2(p.x + size, p.y + size), uv, ImVec2(uv.x + d.x, uv.y + d.y), col);
}
static void pad_names(const char **l, const char **r, const char **a, bool *touchpad, int *fam) {   /* the first pad's own button names and icons */
	*l = "L1"; *r = "R1"; *a = "Cross"; *touchpad = true; *fam = FAM_PS;   /* (the Vita's own names, and a DualShock 4's or DualSense's: unless the pad says otherwise) */
	int n = 0; SDL_JoystickID *ids = SDL_GetGamepads(&n);
	const char *launcher_pad = getenv("RECOMPLAUNCHER_PAD");   /* none connected: RecompLauncher's {pad}, if it gave one */
	if ((ids && n > 0) || (launcher_pad && !strcmp(launcher_pad, "xbox"))) {
		const SDL_GamepadType t = ids && n > 0 ? SDL_GetGamepadTypeForID(ids[0]) : SDL_GAMEPAD_TYPE_XBOXONE;
		if (t == SDL_GAMEPAD_TYPE_XBOX360 || t == SDL_GAMEPAD_TYPE_XBOXONE) { *l = "LB"; *r = "RB"; *a = "A"; *touchpad = false; *fam = FAM_XBOX; }
		else if (t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO || t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR) { *l = "L"; *r = "R"; *a = "B"; *touchpad = false; *fam = FAM_SWITCH; }
		else if (t == SDL_GAMEPAD_TYPE_PS3) *touchpad = false;
	}
	SDL_free(ids);
}
static float ease(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return x * x * (3 - 2 * x); }
/* one device's demonstration at p (its top-left), w x h: its buttons doing the touch, beside a small Vita screen whose
 * marker is the one play draws (build_marks: front blue, rear orange). Played on a loop for the highlighted device,
 * the others at rest with their buttons down; the keyboard's and the pad's loops alternate the front and the rear */
static void intro_demo(ImDrawList *dl, int dev, bool on, int fam, ImVec2 p, float w, float h, double t) {
	const float em = ImGui::GetFontSize(), H = h * 0.74f, sw = h * 1.12f, sh = sw * 544 / 960;
	const ImVec2 s0(p.x + w - sw, p.y + (h - sh) * 0.5f - em * 0.35f), s1(s0.x + sw, s0.y + sh);   /* the screen; its caption below */
	const ImU32 idle = on ? ui::u32(ui::text, 0.92f) : ui::u32(ui::muted, 0.7f), lit = on ? ui::u32(ui::accent) : ui::u32(ui::text, 0.6f);
	const float cy = (s0.y + s1.y) * 0.5f, step = H * 0.8f;   /* the icons' centre line, the screen's; their spacing (cells have margins) */
	const double period = dev == DEV_PAD ? 3.6 : dev == DEV_KBM ? 3.0 : 2.0;
	const float u = on ? (float)fmod(t, period) : dev == DEV_PAD ? 2.1f : dev == DEV_KBM ? 1.3f : 0.7f;   /* (at rest: the moment the touch lands) */
	const bool rear = on && dev != DEV_TOUCH && ((long)(t / period) & 1);
	dl->AddRectFilled(s0, s1, ui::u32(ui::bg, 0.85f), em * 0.25f);
	dl->AddRect(s0, s1, on ? ui::u32(ui::text, 0.35f) : ui::u32(ui::muted, 0.3f), em * 0.25f, 0, 1.0f);
	if (dev != DEV_TOUCH) {
		const char *cap = rear ? "rear" : "front"; const float fs = em * 0.78f;
		const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.0f, cap);
		dl->AddText(ImGui::GetFont(), fs, ImVec2((s0.x + s1.x - ts.x) * 0.5f, s1.y + em * 0.15f), ui::u32(ui::muted, on ? 1.0f : 0.7f), cap);
	}
	ImVec2 m(s0.x + sw * 0.62f, s0.y + sh * 0.45f); bool shown = false, pressed = false, cursor = false; float since = -1;   /* the marker; seconds since it pressed */
	const float ex = s0.x - H * 0.3f;   /* where the icons end */
	if (dev == DEV_KBM) {
#ifdef __APPLE__
		const int key = P_OPTION;
#else
		const int key = P_ALT;
#endif
		const bool held = u >= 0.35f && u < 2.5f, click = held && u >= 1.2f && u < 1.55f;
		const ImVec2 kc(ex - step - H * 0.5f, cy + (held ? H * 0.03f : 0)), mc(ex - H * 0.5f, cy);
		prompt(dl, held ? key : key + 1, kc, H, held ? lit : idle);
		prompt(dl, P_MOUSE, mc, H, idle);
		if (click) prompt(dl, rear ? P_MOUSE_RIGHT : P_MOUSE_LEFT, mc, H, lit);
		if (held) { shown = true; const float k = ease((u - 0.35f) / 0.75f); m = ImVec2(s0.x + sw * (0.3f + 0.32f * k), s0.y + sh * (0.62f - 0.17f * k)); }
		pressed = click; since = u >= 1.2f && held ? u - 1.2f : -1;
	} else if (dev == DEV_PAD) {
		const int base = P_PAD + fam * 7;
		const bool held = u >= 0.3f && u < 3.0f, press = held && u >= 2.0f && u < 2.35f;
		const float mv = u >= 0.7f && u < 1.7f ? sinf((u - 0.7f) * 3.14159f) : 0;   /* the stick pushed, up and to the right, and let go */
		const ImVec2 bc(ex - 2 * step - H * 0.5f, cy + (held ? H * 0.03f : 0)), sc(ex - step - H * 0.5f + mv * H * 0.07f, cy - mv * H * 0.04f), ac(ex - H * 0.5f, cy);
		prompt(dl, base + (rear ? (held ? PAD_R : PAD_R_UP) : (held ? PAD_L : PAD_L_UP)), bc, H, held ? lit : idle);
		prompt(dl, base + PAD_STICK, sc, H, idle);
		prompt(dl, base + (press ? PAD_A : PAD_A_UP), ac, press ? H * 0.92f : H, press ? lit : idle);
		if (held) { shown = cursor = true; const float k = ease((u - 0.7f) / 1.0f); m = ImVec2(s0.x + sw * (0.5f + 0.14f * k), s0.y + sh * (0.5f - 0.08f * k)); }
		pressed = press; since = held && u >= 2.0f ? u - 2.0f : -1;
	} else {   /* a finger over the screen, coming down onto it */
		const float down = u < 0.6f ? ease(u / 0.6f) : u < 0.95f ? 1 : 1 - ease((u - 0.95f) / 0.4f);
		pressed = u >= 0.6f && u < 0.95f; shown = pressed; since = u >= 0.6f ? u - 0.6f : -1;
		const float fh = H * 1.45f * (1.12f - 0.12f * down);   /* (the finger's icon is small in its cell; it shrinks as it comes down) */
		prompt(dl, P_FINGER, ImVec2(m.x + fh * 0.02f, m.y + fh * 0.07f + (1 - down) * H * 0.18f), fh, idle);   /* (the fingertip sits at 0.48, 0.43 of the cell) */
	}
	const ImU32 mc = rear ? IM_COL32(255, 170, 60, 230) : IM_COL32(80, 200, 255, 230);
	const float r = sh * (cursor ? 0.15f : 0.11f);
	if (on && since >= 0 && since < 0.6f) dl->AddCircle(m, r * (1 + since * 3.0f), (mc & 0x00ffffff) | ((ImU32)(200 * (1 - since / 0.6f)) << 24), 0, 1.5f);   /* the touch's ripple */
	if (shown) {
		if (pressed) dl->AddCircleFilled(m, r, (mc & 0x00ffffff) | IM_COL32(0, 0, 0, 120));
		dl->AddCircle(m, r, mc, 0, 1.6f);
		if (cursor) { dl->AddLine(ImVec2(m.x - r * 1.5f, m.y), ImVec2(m.x + r * 1.5f, m.y), mc, 1.0f); dl->AddLine(ImVec2(m.x, m.y - r * 1.5f), ImVec2(m.x, m.y + r * 1.5f), mc, 1.0f); }
	}
}
static void build_intro(void) {
	const double t = g_intro_at >= 0 ? g_intro_at : ImGui::GetTime();
	prompts_load();
	if (g_has_window && (g_intro_dev < 0 || g_intro_done < 0)) g_intro_dev = window_kbm_active() ? DEV_KBM : DEV_PAD;   /* (the device last used, as play sees it; headless, the one asked for) */
	if (g_intro_done < 0 && g_has_window) {   /* a real touch on either panel, through the in-game path: done (its input says which device) */
		int keys[8], xs[8], ys[8];
		for (int port = 0; port < 2 && g_intro_done < 0; port++) {
			const int n = window_touch_contacts(port, keys, xs, ys, 8);
			if (n > 0) { const int k = keys[0]; g_intro_dev = k == 1 ? DEV_KBM : k >= 20 ? DEV_TOUCH : (k >= 4 && k <= 6) ? (window_kbm_active() ? DEV_KBM : DEV_PAD) : DEV_PAD; g_intro_done = t; }
		}
	} else if (g_has_window && g_intro_done >= 0 && t - g_intro_done > 1.2) intro_end();   /* (a moment to see it worked, then the game) */
	const char *pl, *pr, *pa; bool tp; int fam; pad_names(&pl, &pr, &pa, &tp, &fam);
#ifdef __APPLE__
	const char *alt = "Option";
#else
	const char *alt = "Alt";
#endif
	char kbm[256], padb[320];
	snprintf(kbm, sizeof kbm, game_text(GT_I_KBM), alt);
	snprintf(padb, sizeof padb, game_text(GT_I_PAD), pl, pa, pr, tp ? game_text(GT_I_TOUCHPAD) : "");
	const struct { const char *name, *how; } devs[3] = {{game_text(GT_I_KBM_NAME), kbm}, {game_text(GT_I_PAD_NAME), padb}, {game_text(GT_I_TOUCH_NAME), game_text(GT_I_TOUCH)}};
	const ImGuiViewport *vp = ImGui::GetMainViewport(); const float em = ImGui::GetFontSize();
	ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), ui::u32(ui::bg, 0.55f));   /* (the paused game, dimmed behind it) */
	const float cw = fminf(vp->Size.x - 2 * em, 38 * em);
	ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(cw, 0), ImVec2(cw, vp->Size.y - 2 * em));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.4f, em * 1.2f));
	ImGui::Begin("##touchcard", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);   /* (a click anywhere, the card too, is a touch) */
	ui::heading(game_text(g_intro_done < 0 ? GT_I_TITLE : GT_I_DONE), em * 1.35f);
	ui::muted_wrapped(game_text(GT_I_LEAD));
	ImGui::Dummy(ImVec2(0, em * 0.2f));
	const float dh = em * 3.8f, dw = dh * 3.3f;   /* each row's demonstration strip (intro_demo), at its right */
	for (int d = 0; d < 3; d++) {
		const bool on = d == g_intro_dev;
		ImGui::PushStyleColor(ImGuiCol_ChildBg, on ? ui::with_alpha(ui::accent, 0.13f) : ui::card);
		ImGui::PushStyleColor(ImGuiCol_Border, on ? ui::with_alpha(ui::accent, 0.55f) : ui::border);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, em * 0.4f);
		char id[16]; snprintf(id, sizeof id, "##dev%d", d);
		ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders);
		const ImVec2 at = ImGui::GetCursorScreenPos(); const float avail = ImGui::GetContentRegionAvail().x;
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + avail - dw - em);
		ImGui::BeginGroup();
		ImGui::PushStyleColor(ImGuiCol_Text, on ? ui::accent : ui::text); ImGui::TextUnformatted(devs[d].name); ImGui::PopStyleColor();
		ImGui::PushStyleColor(ImGuiCol_Text, on ? ui::text : ui::muted); ImGui::TextWrapped("%s", devs[d].how); ImGui::PopStyleColor();
		ImGui::EndGroup(); ImGui::PopTextWrapPos();
		const float th = ImGui::GetItemRectSize().y;
		if (th < dh) ImGui::Dummy(ImVec2(0, fmaxf(0, dh - th - ImGui::GetStyle().ItemSpacing.y)));
		intro_demo(ImGui::GetWindowDrawList(), d, on, fam, ImVec2(at.x + avail - dw, at.y + (fmaxf(th, dh) - dh) * 0.5f), dw, dh, t);
		ImGui::EndChild();
		ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
	}
	ImGui::Dummy(ImVec2(0, em * 0.2f));
	if (g_intro_done < 0) { ImGui::PushStyleColor(ImGuiCol_Text, ui::accent); ImGui::TextUnformatted(game_text(GT_I_TRY)); ImGui::PopStyleColor(); }
	else ImGui::TextUnformatted(game_text(GT_I_BACK));
	ui::muted_wrapped(game_text(GT_I_FOOT));
	ImGui::End();
	ImGui::PopStyleVar();
}
/* ---- the shader prep card and its corner line (this file's header) */
static void fmt_count(char *out, size_t n, int v) {   /* 5837 -> "5,837" */
	char d[16]; snprintf(d, sizeof d, "%d", v); const int len = (int)strlen(d); size_t o = 0;
	for (int i = 0; i < len && o + 1 < n; i++) { if (i && (len - i) % 3 == 0 && o + 1 < n) out[o++] = ','; out[o++] = d[i]; }
	out[o] = 0;
}
static void build_prep_corner(float w, float hgt, int done, int total) {
	char line[96]; snprintf(line, sizeof line, game_text(GT_P_CORNER), total ? (int)(100.0 * done / total) : 0);
	ImFont *font = ImGui::GetFont(); const float fs = ImGui::GetFontSize(), pad = fs * 0.5f;
	const ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, line);
	const ImVec2 p0(w - sz.x - 3 * pad, hgt - sz.y - 3 * pad), p1(w - pad, hgt - pad);
	ImDrawList *dl = ImGui::GetBackgroundDrawList();   /* (like the HUD) */
	dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 150), fs * 0.3f);
	dl->AddText(font, fs, ImVec2(p0.x + pad, p0.y + pad * 0.6f), IM_COL32(220, 220, 228, 255), line);
	const float bx0 = p0.x + pad, bx1 = p1.x - pad, by = p1.y - pad * 0.7f;
	dl->AddLine(ImVec2(bx0, by), ImVec2(bx1, by), IM_COL32(255, 255, 255, 50), 2.0f);
	if (total) dl->AddLine(ImVec2(bx0, by), ImVec2(bx0 + (bx1 - bx0) * done / total, by), ui::u32(ui::accent, 1.0f), 2.0f);
}
static void build_prep(void) {
	int done, total; double eta; int ps = 1;
	if (g_prep_demo[0] >= 0) { done = g_prep_demo[0]; total = g_prep_demo[1]; eta = g_prep_demo[2]; }
	else ps = gfx_gpu_shader_prep(&done, &total, &eta);
	if (ps != 1) {   /* the builds are done (or the card was never meant): back to the game */
		g_prep_card = false; g_open = false; g_paused = false; apply_pause();
		if (ps == 3) overlay_toast_titled(game_text(GT_T_SHADERS_READY), game_text(GT_SHADERS_READY), 4.0);
		return;
	}
	const ImGuiViewport *vp = ImGui::GetMainViewport(); const float em = ImGui::GetFontSize();
	ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), ui::u32(ui::bg, 0.78f));   /* (the paused game, dimmed behind it) */
	const float cw = fminf(vp->Size.x - 2 * em, 30 * em);
	ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(cw, 0), ImVec2(cw, vp->Size.y - 2 * em));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.4f, em * 1.2f));
	ImGui::Begin("##prepcard", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
	ui::heading(game_text(GT_P_TITLE), em * 1.35f);
	const char *game = std::getenv("RECOMP_GAME_NAME"); char body[384];
	snprintf(body, sizeof body, game_text(GT_P_BODY), game && *game ? game : game_text(GT_THE_GAME));
	ImGui::TextWrapped("%s", body);
	ImGui::Dummy(ImVec2(0, em * 0.3f));
	char a[16], b[16], over[48]; fmt_count(a, sizeof a, done); fmt_count(b, sizeof b, total); snprintf(over, sizeof over, game_text(GT_P_OF), a, b);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ui::accent);
	ImGui::ProgressBar(total ? (float)done / (float)total : 0.0f, ImVec2(-1, em * 1.4f), over);
	ImGui::PopStyleColor();
	char left[96];
	if (eta < 0) snprintf(left, sizeof left, "%s", game_text(GT_P_ETA_UNKNOWN));
	else if (eta < 60) snprintf(left, sizeof left, "%s", game_text(GT_P_ETA_LT1));
	else if (eta <= 60) snprintf(left, sizeof left, "%s", game_text(GT_P_ETA_1));
	else snprintf(left, sizeof left, game_text(GT_P_ETA_N), (int)ceil(eta / 60));
	ui::muted_wrapped(left);
	ImGui::Dummy(ImVec2(0, em * 0.4f));
	if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
	if (ImGui::Button(game_text(GT_P_SKIP))) overlay_close();   /* (closing is Skip: overlay_close) */
	ImGui::Dummy(ImVec2(0, em * 0.2f));
	ui::muted_wrapped(game_text(GT_P_SKIP_NOTE));
	ImGui::End();
	ImGui::PopStyleVar();
}
/* ---- the layout card (overlay_layout_ask): Killzone 1.12 decides at boot whether it plays as a PS TV (every touch action on
 * buttons and sticks, a controller only) or a handheld (touch, with keyboard, mouse or a controller's touch cursor), and
 * keeps it. Touch is the default; the PS TV layout is the player's choice, offered before the title */
static void layout_choose(int v) {
	if (g_layout_remember) { settings_set_int("controller_layout", v); settings_save(); }
	g_layout_card = false; g_layout_ask = 0; g_layout_answer = v;
	g_open = false; g_paused = false; apply_pause();
}
static void build_layout(void) {
	const ImGuiViewport *vp = ImGui::GetMainViewport(); const float em = ImGui::GetFontSize();
	ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), ui::u32(ui::bg, 0.85f));
	const float cw = fminf(vp->Size.x - 2 * em, 34 * em);
	ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(cw, 0), ImVec2(cw, vp->Size.y - 2 * em));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.4f, em * 1.2f));
	ImGui::Begin("##layoutcard", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
	ui::heading("Controller layout", em * 1.35f);
	const char *game = std::getenv("RECOMP_GAME_NAME"); char body[320];
	snprintf(body, sizeof body, "A controller is connected. %s can also play with its own PS TV layout, which puts every touch action on buttons and sticks. It keeps the layout it starts with until you quit.", game && *game ? game : "This game");
	ImGui::TextWrapped("%s", body);
	ImGui::Dummy(ImVec2(0, em * 0.4f));
	const float bw = (ImGui::GetContentRegionAvail().x - em * 0.6f) * 0.5f;
	int chosen = 0;
	ImGui::BeginGroup();
	if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
	if (ui::primary_button("Standard##lc", ImVec2(bw, em * 2.4f))) chosen = 1;
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + bw); ui::muted_wrapped("The game as on a handheld. Keyboard, mouse and controller (its touch cursor) all work."); ImGui::PopTextWrapPos();
	ImGui::EndGroup();
	ImGui::SameLine(0, em * 0.6f);
	ImGui::BeginGroup();
	if (ImGui::Button("PS TV##lc", ImVec2(bw, em * 2.4f))) chosen = 2;
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + bw); ui::muted_wrapped("Controller only: every action on buttons and sticks. Keyboard and mouse don't play the game in it."); ImGui::PopTextWrapPos();
	ImGui::EndGroup();
	ImGui::Dummy(ImVec2(0, em * 0.4f));
	ImGui::Checkbox("Remember my choice##lc", &g_layout_remember);
	ui::muted_wrapped("Change it later in the menu (F1): Controls > Mouse and controller.");
	ImGui::End();
	ImGui::PopStyleVar();
	if (chosen) layout_choose(chosen);
}
extern "C" int overlay_layout_ask(void) {   /* (the game's thread) */
	if (!g_ctx || !g_has_window) return 1;
	g_layout_answer = 0; g_layout_ask = 1;
	int a; while (!(a = g_layout_answer.load())) { struct timespec ts = {0, 10000000}; nanosleep(&ts, nullptr); }
	return a;
}
/* ---- the quit card (this file's header) */
static void build_quit(void) {
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * card_scale(vp->Size.y)); const float em = ImGui::GetFontSize();
	ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), ui::u32(ui::bg, 0.55f));
	const char *game = std::getenv("RECOMP_GAME_NAME"); char title[160];
	snprintf(title, sizeof title, game_text(GT_QUIT), game && *game ? game : game_text(GT_THE_GAME));
	ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(fminf(vp->Size.x - 2 * em, 24 * em), 0), ImVec2(fminf(vp->Size.x - 2 * em, 24 * em), vp->Size.y));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.4f, em * 1.2f));
	ImGui::Begin("##quitcard", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
	ui::heading(title, em * 1.35f);
	ui::muted_wrapped(game_text(GT_QUIT_LOST));
	ImGui::Dummy(ImVec2(0, em * 0.3f));
	if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
	if (ui::primary_button(game_text(GT_KEEP_PLAYING))) overlay_close();
	ImGui::SameLine();
	const bool launched = std::getenv("ABYSS_LAUNCHER") != nullptr;   /* (started by the launcher: back to it, or quit both) */
	if (launched && ImGui::Button(game_text(GT_BACK_LAUNCHER))) g_want_quit = 1;
	if (launched) ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, ui::error);
	if (ImGui::Button(game_text(GT_QUIT_GAME))) g_want_quit = launched ? 2 : 1;
	ImGui::PopStyleColor();
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopFont();
}
void overlay_frame(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, uint32_t w, uint32_t h, double dt_s) {
	if (!overlay_is_open()) return;
	render_frame(cmd, target, w, h, dt_s, g_has_window, g_quit_card ? build_quit : g_layout_card ? build_layout : g_prep_card ? build_prep : build_ui);
}
void overlay_marks(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, uint32_t w, uint32_t h, double dt_s, const OverlayMark *marks, int n, const float rear_rect[4]) {
	if (overlay_is_open()) return;   /* (the menu covers the picture) */
	g_nosd = metrics_osd_lines(g_osd, 6);
	/* headless (the dump's HUD check): only with something to show. Windowed: every present -- the toasts'
	 * timers, the cursor and the capture flag live in this frame (a frame that draws nothing is
	 * a few microseconds of CPU and no render pass: render_frame) */
	if (!g_has_window && !n && !g_nosd && !(rear_rect && rear_rect[2] > 0) && !(g_focus[2] > 0)) return;
	g_marks = marks; g_nmarks = n;
	for (int i = 0; i < 4; i++) g_rear[i] = rear_rect ? rear_rect[i] : 0;
	render_frame(cmd, target, w, h, dt_s, g_has_window, build_closed);
}
/* RECOMP_OVERLAY_DUMP (window.c): deliberately bypasses overlay_is_open() -- this must never be the
 * thing that makes the rest of the host think the menu is open (display.c's pause gate, window.c's
 * input routing and audio mute all key off that flag, not off anything here). `ui` (overlay.h): NULL
 * or "menu" the menu (the launcher's pages); "toast" the first-launch hint; the cards by name. */
/* one headless frame of `build`, on a command buffer of its own when drawn (the SDLGPU3 backend keeps one
 * vertex/index buffer set and one texture-upload path per frame: one drawn frame per command buffer, as
 * the interactive present loop does, keeps a later frame's uploads out of an earlier frame's draws) */
static void forced_frame(SDL_GPUDevice *device, SDL_GPUTexture *target, uint32_t w, uint32_t h, bool draw, void (*build)(void)) {
	g_skip_draw = !draw;
	SDL_GPUCommandBuffer *cmd = draw ? SDL_AcquireGPUCommandBuffer(device) : nullptr;
	if (draw && !cmd) { g_skip_draw = false; return; }
	render_frame(cmd, target, w, h, 0, false, build);
	if (cmd) SDL_SubmitGPUCommandBuffer(cmd);
	g_skip_draw = false;
}
void overlay_render_forced(SDL_GPUDevice *device, SDL_GPUTexture *target, uint32_t w, uint32_t h, const char *ui) {
	if (!g_ctx || !device) return;
	g_nmarks = 0; g_rear[2] = 0; g_nosd = 0;
	if (!ui || !*ui || !strncmp(ui, "menu", 4)) {
		/* the launcher's pages (ABYSS_LAUNCHER_PAGE=1..2 and ABYSS_LAUNCHER_TAB=n choose one, as for the launcher's own
		 * shots): 2.5 s of 1/60 s frames, the last one drawn, so its entrance is over */
		launcher_game_open();
		for (int i = 0; i < 150; i++) forced_frame(device, target, w, h, i == 149, build_ui);
		return;
	}
	if (!strcmp(ui, "quit")) { for (int i = 0; i < 3; i++) forced_frame(device, target, w, h, i == 2, build_quit); return; }   /* (the quit card) */
	if (!strcmp(ui, "layout")) { for (int i = 0; i < 3; i++) forced_frame(device, target, w, h, i == 2, build_layout); return; }   /* (the layout card: RECOMP_GAME_NAME names the game) */
	if (!strcmp(ui, "prep")) { g_prep_demo[0] = 1240; g_prep_demo[1] = 5837; g_prep_demo[2] = 372; for (int i = 0; i < 3; i++) forced_frame(device, target, w, h, i == 2, build_prep); g_prep_demo[0] = -1; return; }   /* (the shader prep card, example numbers) */
	if (!strncmp(ui, "intro", 5)) {   /* "intro", "intro:pad", "intro:touch": the touch card with that device highlighted; "@<seconds>" after it, that moment of its loop */
		const char *at = strchr(ui, '@');
		g_intro_dev = !strncmp(ui, "intro:pad", 9) ? DEV_PAD : !strncmp(ui, "intro:touch", 11) ? DEV_TOUCH : DEV_KBM; g_intro_done = -1; g_intro_at = at ? atof(at + 1) : 0;
		for (int i = 0; i < 3; i++) forced_frame(device, target, w, h, i == 2, build_intro);
		g_intro_dev = -1; g_intro_at = -1;
		return;
	}
	if (!strcmp(ui, "toast")) g_force_toast = true;
	g_nosd = metrics_osd_lines(g_osd, 6);   /* the HUD too (RECOMP_OSD) */
	for (int i = 0; i < 3; i++) forced_frame(device, target, w, h, i == 2, build_closed);   /* (the toasts' entrance: drawn on the third) */
	g_nosd = 0;
}
