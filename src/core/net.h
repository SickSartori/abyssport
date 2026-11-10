/* Stand-in for Project Nativ's net.h: Golden Abyss has no online play, so abyssport has no network part.
 * What the rest of the runtime asks of it (net.c). */
#ifndef ABYSS_NET_H
#define ABYSS_NET_H
#ifdef __cplusplus
extern "C" {
#endif
int net_online(void);                  /* 0: never online */
const char *net_online_needs(void);    /* 0: no update is needed for online play */
int matching_local(void);              /* 0: no matching service */
#ifdef __cplusplus
}
#endif
#endif
