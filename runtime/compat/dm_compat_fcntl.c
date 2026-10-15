/*
 * Dispatch for the fcntl redirection in darwin/fcntl_ext.h.
 *
 * The header macro-renames fcntl, so this file undoes that for its own calls
 * and reaches the real libc entry point.
 */
#undef fcntl
#include <fcntl.h>

#include <darwin/fcntl_ext.h>

int dm_compat_fcntl_impl(int fd, int cmd, void *arg) {
    if (cmd == F_GETPATH)
        return dm_compat_getpath(fd, (char *)arg, DM_COMPAT_MAXPATHLEN);

    /* Bionic's fcntl takes an optional third argument and ignores it for the
     * commands that do not use one, so passing it through is safe here. */
    return fcntl(fd, cmd, arg);
}
