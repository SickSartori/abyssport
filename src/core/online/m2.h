/* Stand-in for Project Nativ's online/m2.h (abyssport has no network part, net.h): what the overlay's page asks. */
#ifndef ABYSS_ONLINE_M2_H
#define ABYSS_ONLINE_M2_H
typedef struct { int started, port, linked, known, dht_nodes, rooms_heard, rooms_mine, clash; char name[17], pub[32]; } OnlineStatus;
void online_status(OnlineStatus *s);   /* all zero: never started */
#endif
