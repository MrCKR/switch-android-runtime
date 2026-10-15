/*
 * sys/xattr.h — Darwin extended-attribute API over Linux xattr.
 *
 * Reached by shadowing the system header through include-path order, because
 * upstream includes <sys/xattr.h> itself (hle_fs.c).
 *
 * The difference is the signature, not the capability: Darwin takes a position
 * argument and an options mask, Linux takes neither. Both are supplied here and
 * the ones Linux cannot express are handled explicitly:
 *
 *   position != 0  ->  not representable on Linux; report ENOTSUP rather than
 *                      silently returning the whole attribute
 *   XATTR_NOFOLLOW ->  maps to lgetxattr/lsetxattr/lremovexattr
 *
 * ENOATTR is Darwin's missing-attribute errno; Linux spells it ENODATA.
 */
#ifndef DM_COMPAT_SYS_XATTR_H
#define DM_COMPAT_SYS_XATTR_H

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#define XATTR_NOFOLLOW 0x0001
#define XATTR_CREATE 0x0002
#define XATTR_REPLACE 0x0004

ssize_t dm_compat_getxattr(const char *path, const char *name, void *value,
                           size_t size, uint32_t position, int options);
int dm_compat_setxattr(const char *path, const char *name, const void *value,
                       size_t size, uint32_t position, int options);
int dm_compat_removexattr(const char *path, const char *name, int options);

/* Upstream calls these names directly. */
#define getxattr(path, name, value, size, position, options) \
    dm_compat_getxattr((path), (name), (value), (size), (position), (options))
#define setxattr(path, name, value, size, position, options) \
    dm_compat_setxattr((path), (name), (value), (size), (position), (options))
#define removexattr(path, name, options) \
    dm_compat_removexattr((path), (name), (options))

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_SYS_XATTR_H */
