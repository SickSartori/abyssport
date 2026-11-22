/*
 * Adapted from Project Nativ (https://github.com/TheLostPantheon/nativ), runtime/ui/ui_theme.h at 4b1c428:
 * GNU General Public License, version 2 or (at your option) any later version.
 */
/*
 * The cards' and toasts' look in the game (overlay.cpp): abyssport's launcher's (src/launcher/launcher.cpp, whose pages
 * are the game's menu), on Dear ImGui's own widgets -- its warm black and gold, square corners, Barlow Semi Condensed
 * (the launcher's letters, in the same atlas: launcher_game_init), titles in heavy italic capitals, the one primary
 * button a card offers leaning like the launcher's. Header-only C++.
 */
#ifndef ATIV_UI_THEME_H
#define ATIV_UI_THEME_H
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <sys/stat.h>

namespace ui {
/* the launcher's colours (launcher.cpp kGold, kGoldDark, kInk, kDim, kGreen, kAmber; its panel 14,12,10) */
inline const ImVec4 bg(0.039f, 0.031f, 0.024f, 1), surface(0.055f, 0.047f, 0.039f, 0.97f), card(1, 1, 1, 0.07f),
	card_hi(1, 1, 1, 0.12f), border(1, 1, 1, 0.10f), text(0.949f, 0.945f, 0.933f, 1), muted(0.667f, 0.651f, 0.620f, 1),
	accent(0.886f, 0.659f, 0.173f, 1), accent_hi(1.0f, 0.808f, 0.376f, 1), accent_dark(0.588f, 0.376f, 0.055f, 1), accent_ink(0.071f, 0.055f, 0.024f, 1),
	ready(0.431f, 0.890f, 0.431f, 1), warn(1.0f, 0.698f, 0.118f, 1), error(1.0f, 0.471f, 0.392f, 1);
inline ImVec4 with_alpha(const ImVec4 &c, float a) { return ImVec4(c.x, c.y, c.z, c.w * a); }
inline ImU32 u32(const ImVec4 &c, float a = 1) { return ImGui::GetColorU32(with_alpha(c, a)); }

/* the style: sizes in points at font_size, scaled by the display's content scale */
inline void apply(float font_size, float scale) {
	ImGuiStyle &st = ImGui::GetStyle();
	ImGui::StyleColorsDark();
	st.FontSizeBase = font_size; st.FontScaleDpi = scale;
	const float k = font_size / 16.0f;
	st.WindowPadding = ImVec2(22 * k, 18 * k); st.FramePadding = ImVec2(14 * k, 7 * k); st.ItemSpacing = ImVec2(12 * k, 9 * k);
	st.ItemInnerSpacing = ImVec2(8 * k, 6 * k); st.CellPadding = ImVec2(10 * k, 7 * k); st.SeparatorTextPadding = ImVec2(0, 6 * k);
	st.WindowRounding = 0; st.ChildRounding = 0; st.FrameRounding = 0; st.PopupRounding = 0; st.GrabRounding = 0;   /* (square, as the launcher) */
	st.ScrollbarRounding = 0; st.TabRounding = 0; st.GrabMinSize = 14 * k; st.ScrollbarSize = 11 * k;
	st.WindowBorderSize = 1; st.ChildBorderSize = 1; st.PopupBorderSize = 1; st.FrameBorderSize = 0; st.TabBorderSize = 0;
	ImVec4 *c = st.Colors;
	c[ImGuiCol_Text] = text; c[ImGuiCol_TextDisabled] = muted; c[ImGuiCol_WindowBg] = bg; c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_PopupBg] = surface; c[ImGuiCol_Border] = border; c[ImGuiCol_FrameBg] = card; c[ImGuiCol_FrameBgHovered] = card_hi;
	c[ImGuiCol_FrameBgActive] = card_hi; c[ImGuiCol_Button] = card; c[ImGuiCol_ButtonHovered] = with_alpha(accent, 0.6f);
	c[ImGuiCol_ButtonActive] = accent_dark; c[ImGuiCol_Header] = accent; c[ImGuiCol_HeaderHovered] = with_alpha(accent, 0.7f);
	c[ImGuiCol_HeaderActive] = accent_dark; c[ImGuiCol_Separator] = border; c[ImGuiCol_SeparatorHovered] = border;
	c[ImGuiCol_CheckMark] = accent; c[ImGuiCol_SliderGrab] = accent; c[ImGuiCol_SliderGrabActive] = accent_hi; c[ImGuiCol_TextLink] = accent;
	c[ImGuiCol_TitleBg] = surface; c[ImGuiCol_TitleBgActive] = surface; c[ImGuiCol_MenuBarBg] = surface; c[ImGuiCol_Tab] = card;
	c[ImGuiCol_TabHovered] = card_hi; c[ImGuiCol_TabSelected] = card_hi; c[ImGuiCol_TabSelectedOverline] = accent;
	c[ImGuiCol_TableHeaderBg] = card; c[ImGuiCol_TableBorderLight] = border; c[ImGuiCol_TableBorderStrong] = border;
	c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0); c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.025f);
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.02f, 0.02f, 0.03f, 0.72f); c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
	c[ImGuiCol_ScrollbarGrab] = card_hi; c[ImGuiCol_NavCursor] = accent; c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
	st.ScaleAllSizes(scale);
}
/* one of the launcher's faces (launcher.cpp load_fonts: BarlowSemiCondensed-<style>.ttf) in the atlas, by its file's
 * name; null when the launcher's letters aren't there (then the default face) */
inline ImFont *face(const char *style) {
	for (ImFont *f : ImGui::GetIO().Fonts->Fonts) if (std::strstr(f->GetDebugName(), style)) return f;
	return nullptr;
}
/* a parallelogram leaning right, as the launcher's (launcher.cpp skewed_rect) */
inline void lean_rect(ImDrawList *dl, ImVec2 a, ImVec2 b, ImU32 c) {
	const float s = (b.y - a.y) * 0.21f;
	dl->AddQuadFilled(ImVec2(a.x + s, a.y), ImVec2(b.x + s, a.y), ImVec2(b.x, b.y), ImVec2(a.x, b.y), c);
}

/* the system's own UI face, read at run time (nothing shipped): San Francisco, Segoe UI, a common Linux sans;
 * ImGui's built-in vector font when none is there. ImGui 1.92's fonts size themselves: one face, any size */
inline ImFont *load_system_font(ImGuiIO &io) {
	static const char *const faces[] = {"/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/Helvetica.ttc", "C:\\Windows\\Fonts\\segoeui.ttf",
		"/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/cantarell/Cantarell-VF.otf", 0};
	for (int i = 0; faces[i]; i++) { struct stat st; if (stat(faces[i], &st) == 0) if (ImFont *f = io.Fonts->AddFontFromFileTTF(faces[i])) return f; }
	return io.Fonts->AddFontDefaultVector();
}

inline bool primary_button(const char *label, ImVec2 size = ImVec2(0, 0)) {   /* the one gold button a card offers, leaning */
	const char *end = ImGui::FindRenderedTextEnd(label);
	const ImVec2 ts = ImGui::CalcTextSize(label, end), pad = ImGui::GetStyle().FramePadding;
	if (size.x <= 0) size.x = ts.x + pad.x * 3;
	if (size.y <= 0) size.y = ts.y + pad.y * 2;
	const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
	const bool r = ImGui::Button(label, size);   /* (for the id, focus and keys: drawn over below) */
	ImGui::PopStyleColor(4);
	const bool hot = ImGui::IsItemHovered() || ImGui::IsItemFocused();
	ImDrawList *dl = ImGui::GetWindowDrawList();
	lean_rect(dl, a, ImVec2(b.x - size.y * 0.21f, b.y), u32(hot ? accent_hi : accent));
	dl->AddText(ImVec2(a.x + (size.x - ts.x) * 0.5f, a.y + (size.y - ts.y) * 0.5f), u32(accent_ink), label, end);
	return r;
}
/* a choice of n as one joined control; true when it changes. ids: labels may carry "##" suffixes */
inline bool segmented(const char *const *labels, int n, int *v) {
	bool ch = false;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x * 0.25f, 0));
	for (int i = 0; i < n; i++) {
		if (i) ImGui::SameLine();
		bool on = *v == i;
		ImGui::PushStyleColor(ImGuiCol_Button, on ? accent : card);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on ? accent_hi : card_hi);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent_dark);
		ImGui::PushStyleColor(ImGuiCol_Text, on ? accent_ink : muted);
		if (ImGui::Button(labels[i]) && !on) { *v = i; ch = true; }
		ImGui::PopStyleColor(4);
	}
	ImGui::PopStyleVar();
	return ch;
}
inline bool toggle(const char *id, bool *on) {   /* Off / On as a segmented pair */
	char a[64], b[64]; snprintf(a, sizeof a, "Off##%s", id); snprintf(b, sizeof b, "On##%s", id);
	const char *const ls[2] = {a, b};
	int v = *on ? 1 : 0;
	if (!segmented(ls, 2, &v)) return false;
	*on = v != 0;
	return true;
}
/* a card's title: the launcher's heavy italic, in capitals */
inline void heading(const char *s, float size) {
	char up[160]; std::snprintf(up, sizeof up, "%s", s);
	for (char *c = up; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);   /* (ASCII only: accented letters stay) */
	ImGui::PushFont(face("BlackItalic"), size * 1.25f); ImGui::TextUnformatted(up); ImGui::PopFont();
}
inline void muted_wrapped(const char *s) { ImGui::PushStyleColor(ImGuiCol_Text, muted); ImGui::TextWrapped("%s", s); ImGui::PopStyleColor(); }
inline void dot(const ImVec4 &c) {   /* a status dot, centred on the text line */
	ImVec2 p = ImGui::GetCursorScreenPos(); float h = ImGui::GetTextLineHeight(), r = h * 0.22f;
	ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + r, p.y + h * 0.5f), r, u32(c));
	ImGui::Dummy(ImVec2(r * 2, h)); ImGui::SameLine(0, h * 0.35f);
}
inline void spinner(const ImVec4 &c) {
	ImVec2 p = ImGui::GetCursorScreenPos(); float h = ImGui::GetTextLineHeight(), r = h * 0.36f, t = (float)ImGui::GetTime() * 6.0f;
	ImDrawList *dl = ImGui::GetWindowDrawList();
	dl->PathArcTo(ImVec2(p.x + r, p.y + h * 0.5f), r, t, t + 4.4f, 20); dl->PathStroke(u32(c), 0, r * 0.32f);
	ImGui::Dummy(ImVec2(r * 2, h)); ImGui::SameLine(0, h * 0.4f);
}
inline void chip(const char *s, const ImVec4 &c) {   /* a small rounded label, tinted */
	ImVec2 p = ImGui::GetCursorScreenPos(), ts = ImGui::CalcTextSize(s); float px = ImGui::GetFontSize() * 0.55f, py = ImGui::GetFontSize() * 0.18f;
	ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + ts.x + px * 2, p.y + ts.y + py * 2), u32(c, 0.16f));
	ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + px, p.y + py), u32(c), s);
	ImGui::Dummy(ImVec2(ts.x + px * 2, ts.y + py * 2));
}

/* settings rows: rows_begin, then per setting row(label, explanation) and its control(s), then rows_end */
inline bool rows_begin(const char *id) {
	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) return false;
	ImGui::TableSetupColumn("setting", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch, 1.15f);
	return true;
}
inline void row(const char *label, const char *explanation = nullptr) {
	ImGui::TableNextRow(0, ImGui::GetFrameHeight() + ImGui::GetStyle().CellPadding.y * 2);
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(label);
	if (explanation && *explanation) { ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.88f); muted_wrapped(explanation); ImGui::PopFont(); }
	ImGui::TableNextColumn();
}
inline void rows_end(void) { ImGui::EndTable(); }

/* the Vita in outline, `width` px wide (its units 600 x 280): the screen filled `screen`; controls 0 none, 1 the
 * sticks as dots (small sizes), 2 d-pad left, face buttons right, the two sticks; returns the height */
inline float vita(ImDrawList *dl, ImVec2 at, float width, ImU32 line, ImU32 screen, float body_w = 14, int controls = 2) {
	const float s = width / 600.0f;
	auto P = [&](float x, float y) { return ImVec2(at.x + x * s, at.y + y * s); };
	dl->AddRectFilled(P(125, 41), P(475, 239), screen, 12 * s);
	dl->AddRect(P(8, 8), P(592, 272), line, 132 * s, 0, body_w * s);
	if (controls == 1) { dl->AddCircleFilled(P(70, 140), 22 * s, line); dl->AddCircleFilled(P(530, 140), 22 * s, line); }
	if (controls == 2) {
		const float k = 12 * s;
		dl->AddLine(P(50, 108), P(82, 108), line, k); dl->AddLine(P(66, 92), P(66, 124), line, k);
		for (ImVec2 c : {P(50, 108), P(82, 108), P(66, 92), P(66, 124)}) dl->AddCircleFilled(c, k * 0.5f, line);
		dl->AddCircle(P(74, 190), 14 * s, line, 0, 10 * s); dl->AddCircle(P(526, 190), 14 * s, line, 0, 10 * s);
		for (ImVec2 c : {P(534, 90), P(516, 108), P(552, 108), P(534, 126)}) dl->AddCircleFilled(c, 8 * s, line);
	}
	return 280 * s;
}
}   /* namespace ui */
#endif
