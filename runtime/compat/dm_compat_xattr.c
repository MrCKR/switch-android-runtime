/*
 * Linux xattr syscall backing for darwin/sys/xattr.h.
 *
 * Android's public NDK headers do not expose sys/xattr.h, so call the Linux
 * syscalls directly. The runtime still receives the Darwin-shaped API from the
 * compatibility header; only this implementation knows the Linux ABI.
 */
#include <errno.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

#undef getxattr
#undef setxattr
#undef removexattr

#ifndef SYS_getxattr
#define SYS_getxattr  getxattr
#endif
#ifndef SYS_lgetxattr
#define SYS_lgetxattr lgetxattr
#endif
#ifndef SYS_setxattr
#define SYS_setxattr  setxattr
#endif
#ifndef SYS_lsetxattr
#define SYS_lsetxattr lsetxattr
#endif
#ifndef SYS_removexattr
#define SYS_removexattr removexattr
#endif
#ifndef SYS_lremovexattr
#define SYS_lremovexattr lremovexattr
#endif

ssize_t dm_compat_getxattr(const char *path, const char *name, void *value,
                           size_t size, uint32_t position, int options) {
    if (position != 0) { errno = ENOTSUP; return -1; }
    long r;
    if (options & XATTR_NOFOLLOW)
        r = syscall(SYS_lgetxattr, path, name, value, size);
    else
        r = syscall(SYS_getxattr, path, name, value, size);
    return (ssize_t)r;
}

int dm_compat_setxattr(const char *path, const char *name, const void *value,
                       size_t size, uint32_t position, int options) {
    if (position != 0) { errno = ENOTSUP; return -1; }
    int flags = 0;
    if (options & XATTR_CREATE) flags |= XATTR_CREATE;
    if (options & XATTR_REPLACE) flags |= XATTR_REPLACE;
    long r;
    if (options & XATTR_NOFOLLOW)
        r = syscall(SYS_lsetxattr, path, name, value, size, flags);
    else
        r = syscall(SYS_setxattr, path, name, value, size, flags);
    return (int)r;
}

int dm_compat_removexattr(const char *path, const char *name, int options) {
    long r = (options & XATTR_NOFOLLOW)
        ? syscall(SYS_lremovexattr, path, name)
        : syscall(SYS_removexattr, path, name);
    return (int)r;
}
