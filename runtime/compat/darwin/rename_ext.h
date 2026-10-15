/*
 * rename extensions — Darwin renamex_np over Linux renameat2.
 *
 * This one is a real capability match, not a downgrade. Upstream's save commit
 * relies on RENAME_SWAP to swap the working copy and the committed copy
 * atomically (hle_fs.c: Commit). Linux renameat2(2) with RENAME_EXCHANGE gives
 * the same guarantee, so the atomicity upstream depends on is preserved.
 */
#ifndef DM_COMPAT_RENAME_EXT_H
#define DM_COMPAT_RENAME_EXT_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RENAME_SWAP
#define RENAME_SWAP 0x00000002
#endif
#ifndef RENAME_EXCL
#define RENAME_EXCL 0x00000004
#endif

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif

static inline int dm_compat_rename_swap(const char *a, const char *b) {
#ifdef SYS_renameat2
    return (int)syscall(SYS_renameat2, AT_FDCWD, a, AT_FDCWD, b, RENAME_EXCHANGE);
#else
    errno = ENOSYS;
    return -1;
#endif
}

static inline int dm_compat_rename_excl(const char *a, const char *b) {
#ifdef SYS_renameat2
    return (int)syscall(SYS_renameat2, AT_FDCWD, a, AT_FDCWD, b, RENAME_NOREPLACE);
#else
    errno = ENOSYS;
    return -1;
#endif
}

/*
 * The wrappers upstream calls by name. Only the flags it actually uses are
 * implemented; anything else reports ENOTSUP instead of picking a wrong
 * semantic.
 */
static inline int renamex_np(const char *a, const char *b, unsigned int flags) {
    if (flags & RENAME_SWAP) return dm_compat_rename_swap(a, b);
    if (flags & RENAME_EXCL) return dm_compat_rename_excl(a, b);
    if (flags == 0) return rename(a, b);
    errno = ENOTSUP;
    return -1;
}

static inline int renameatx_np(int fromfd, const char *a, int tofd,
                               const char *b, unsigned int flags) {
    (void)fromfd; (void)tofd;
    return renamex_np(a, b, flags);
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_RENAME_EXT_H */
