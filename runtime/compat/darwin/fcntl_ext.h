/*
 * fcntl extensions — Darwin/POSIX shim for Android.
 *
 * Upstream uses two fcntl commands in the host layer:
 *
 *   F_GETPATH  hle_fdstat.c — turn a descriptor into its path when classifying
 *              open descriptors. Darwin-only; Bionic has no such command, but
 *              /proc/self/fd/<n> carries the same information.
 *   F_GETFD    diag.c — check whether a descriptor is open. Portable.
 *
 * Only F_GETPATH needs a shim.
 *
 * The macro funnels both call shapes into one pointer argument. Writing it as
 * a comma expression, (void *)(0 __VA_ARGS__), means the no-argument call
 * (fcntl(fd, F_GETFD)) becomes a null pointer and the one-argument call
 * becomes that argument — without reading a va_arg that was never supplied,
 * which would be undefined behaviour.
 */
#ifndef DM_COMPAT_FCNTL_EXT_H
#define DM_COMPAT_FCNTL_EXT_H

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef F_GETPATH
#define F_GETPATH 50
#endif

/* Darwin's F_GETPATH buffer limit; upstream's buffer is MAXPATHLEN. */
#define DM_COMPAT_MAXPATHLEN 1024

/*
 * Read the path of fd into buf. Upstream treats a non-zero return as failure
 * and expects an empty string in that case.
 */
static inline int dm_compat_getpath(int fd, char *buf, int buflen) {
    if (!buf || buflen <= 0) return -1;
    char link[64];
    int n = snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    if (n <= 0 || n >= (int)sizeof link) return -1;
    ssize_t k = readlink(link, buf, (size_t)buflen - 1);
    if (k <= 0) { buf[0] = '\0'; return -1; }
    buf[k] = '\0';
    return 0;
}

/*
 * Single dispatch point. Defined in dm_compat_fcntl.c so that this header can
 * macro-rename fcntl without the implementation recursing.
 */
int dm_compat_fcntl_impl(int fd, int cmd, void *arg);

#define fcntl(fd, cmd, ...) \
    dm_compat_fcntl_impl((fd), (cmd), (void *)(0, ##__VA_ARGS__))

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_FCNTL_EXT_H */
