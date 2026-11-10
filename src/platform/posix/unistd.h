/* <unistd.h> for the runtime's POSIX code on Windows (posix.c): only what is used. */
#ifndef ABYSS_POSIX_UNISTD_H
#define ABYSS_POSIX_UNISTD_H
#include <io.h>   /* (dup, close) */
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned useconds_t;
int usleep(useconds_t us);
#ifdef __cplusplus
}
#endif
#endif
