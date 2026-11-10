/* <strings.h> for the runtime's POSIX code on Windows. */
#ifndef ABYSS_POSIX_STRINGS_H
#define ABYSS_POSIX_STRINGS_H
#include <string.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif
