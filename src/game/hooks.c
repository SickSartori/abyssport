/*
 * The game's patches: the functions the recompiled code calls at the addresses hooked when it was generated
 * (field of view, wider aspect ratios, 60 fps, mouse look, view distance).
 * shortcut: all empty for now, so the game behaves as on the console; each gets its body from Project Nativ's
 * packs/PCSA00029/hooks.c when the runtime it relies on (settings, input, the frame clock) exists.
 */
#include "rt.h"

#define HOOK(name) void recomp_hook_##name(Cpu *c) { (void)c; }
#define IHOOK(name) void recomp_ihook_##name(Cpu *c, int reg) { (void)c; (void)reg; }

HOOK(uc_balance_tick) HOOK(uc_gesture_end) HOOK(uc_gesture_update) HOOK(uc_items_reset)
HOOK(uc_proj) HOOK(uc_pvs_free) HOOK(uc_pvs_new) HOOK(uc_slide_tick)

IHOOK(frame_dt) IHOOK(frames30) IHOOK(uc_aspect) IHOOK(uc_fov_copy) IHOOK(uc_item_pools)
IHOOK(uc_look_velocity) IHOOK(uc_mouse_look) IHOOK(uc_path_end) IHOOK(uc_pvs_union)
IHOOK(uc_rub_wait_floor) IHOOK(uc_shape_score) IHOOK(uc_shape_threshold) IHOOK(uc_zone_done)
