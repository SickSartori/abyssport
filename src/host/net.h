/* Stand-in for Nativ's net.h: Golden Abyss has no online play, so there is no network part. What apputil.c asks. */
#ifndef ABYSS_NET_H
#define ABYSS_NET_H
static inline int net_online(void) { return 0; }
static inline const char *net_online_needs(void) { return 0; }
#endif
