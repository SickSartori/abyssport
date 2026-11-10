/* Stand-in for Nativ's overlay.h until there is an overlay: what apputil.c asks of it (pending.c). */
#ifndef ABYSS_OVERLAY_H
#define ABYSS_OVERLAY_H
void overlay_toast_titled(const char *title, const char *body, double seconds);
int overlay_draws_hud(void);
#endif
