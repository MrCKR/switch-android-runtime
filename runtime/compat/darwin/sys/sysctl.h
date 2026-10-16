/* Android/Bionic stand-in for the small sysctl surface used by upstream. */
#ifndef DM_COMPAT_SYS_SYSCTL_H
#define DM_COMPAT_SYS_SYSCTL_H

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define CTL_KERN 1
#define KERN_PROC 14
#define KERN_PROC_PID 1

struct kinfo_proc {
    struct {
        struct timeval p_starttime;
    } kp_proc;
};

/* Android has no BSD sysctl tree. Returning failure makes the caller use its
 * already-correct monotonic-clock fallback. */
static inline int sysctl(const int *name, unsigned namelen, void *oldp,
                         size_t *oldlenp, const void *newp, size_t newlen) {
    (void)name; (void)namelen; (void)oldp; (void)oldlenp;
    (void)newp; (void)newlen;
    return -1;
}

#endif
