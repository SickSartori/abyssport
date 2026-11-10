/* <dirent.h> for the runtime's POSIX code on Windows (posix.c): a directory's names, "." and ".." included. */
#ifndef ABYSS_POSIX_DIRENT_H
#define ABYSS_POSIX_DIRENT_H
#ifdef __cplusplus
extern "C" {
#endif
struct dirent { char d_name[260]; };
typedef struct DIR DIR;
DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
#ifdef __cplusplus
}
#endif
#endif
