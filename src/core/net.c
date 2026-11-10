/* Golden Abyss has no online play: the network part of Project Nativ's runtime is not in abyssport (net.h). */
#include "net.h"

int net_online(void) { return 0; }
const char *net_online_needs(void) { return 0; }
int matching_local(void) { return 0; }

#include <string.h>
#include "online/m2.h"
void online_status(OnlineStatus *s) { memset(s, 0, sizeof *s); }
