/*
 * sys/clonefile.h — APFS copy-on-write shim for Android.
 *
 * Upstream uses clonefile to make the save-directory working copy nearly free
 * (hle_fs.c: "commit" clones the whole tree into <commit>.work, the game writes
 * to the working copy, and Commit swaps them atomically).
 *
 * Android's filesystems have no reflink clone exposed through a portable API,
 * so the copy is a real recursive copy. This is a genuine capability
 * difference, not just a missing declaration:
 *
 *   APFS:   clone = O(1) metadata operation, blocks shared until written
 *   Android: clone = O(size) byte copy
 *
 * The save tree is small and this runs at mount / commit, not per frame, so
 * the cost is acceptable — but it is recorded rather than hidden. Upstream's
 * atomic-swap half of the same flow *is* preserved exactly, because Linux
 * renameat2(RENAME_EXCHANGE) provides the same guarantee (see rename_ext.h).
 *
 * The trailing COPYFILE_* flags upstream may pass are accepted and ignored.
 */
#ifndef DM_COMPAT_SYS_CLONEFILE_H
#define DM_COMPAT_SYS_CLONEFILE_H

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLONE_NOFOLLOW 0x0001
#define CLONE_NOOWNERCOPY 0x0002

/* Recursive copy of src into dst (dst must not exist), matching clonefile. */
static inline int dm_compat_copy_tree(const char *src, const char *dst, int depth) {
    if (depth > 64) { errno = ELOOP; return -1; }

    struct stat st;
    if (lstat(src, &st) != 0) return -1;

    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, st.st_mode & 07777) != 0) return -1;

        DIR *d = opendir(src);
        if (!d) return -1;
        struct dirent *e;
        int rc = 0;
        while ((e = readdir(d)) != NULL) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char a[4096], b[4096];
            if (snprintf(a, sizeof a, "%s/%s", src, e->d_name) >= (int)sizeof a ||
                snprintf(b, sizeof b, "%s/%s", dst, e->d_name) >= (int)sizeof b) {
                rc = -1; errno = ENAMETOOLONG; break;
            }
            if (dm_compat_copy_tree(a, b, depth + 1) != 0) { rc = -1; break; }
        }
        closedir(d);
        return rc;
    }

    if (S_ISLNK(st.st_mode)) {
        char target[4096];
        ssize_t n = readlink(src, target, sizeof target - 1);
        if (n < 0) return -1;
        target[n] = '\0';
        return symlink(target, dst);
    }

    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    int out = open(dst, O_WRONLY | O_CREAT | O_EXCL, st.st_mode & 07777);
    if (out < 0) { close(in); return -1; }

    char buf[65536];
    int rc = 0;
    for (;;) {
        ssize_t r = read(in, buf, sizeof buf);
        if (r == 0) break;
        if (r < 0) { rc = -1; break; }
        ssize_t off = 0;
        while (off < r) {
            ssize_t w = write(out, buf + off, (size_t)(r - off));
            if (w <= 0) { rc = -1; break; }
            off += w;
        }
        if (rc != 0) break;
    }
    close(in);
    close(out);
    return rc;
}

static inline int clonefile(const char *src, const char *dst, int flags) {
    (void)flags;
    if (!src || !dst) { errno = EINVAL; return -1; }
    return dm_compat_copy_tree(src, dst, 0);
}

static inline int clonefileat(int src_dirfd, const char *src, int dst_dirfd,
                              const char *dst, int flags) {
    (void)src_dirfd; (void)dst_dirfd;
    return clonefile(src, dst, flags);
}

static inline int fclonefileat(int srcfd, int dst_dirfd, const char *dst, int flags) {
    (void)srcfd; (void)dst_dirfd; (void)dst; (void)flags;
    errno = ENOTSUP;
    return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_SYS_CLONEFILE_H */
