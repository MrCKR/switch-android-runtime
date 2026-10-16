/*
 * platform_compat.h — umbrella header for the Darwin shims.
 *
 * Force-included into every upstream translation unit (see
 * tools/compile_upstream_modules.sh). Upstream includes some Darwin headers
 * but not all, and editing upstream is off the table, so the umbrella is how
 * a shim reaches a file that never asked for it.
 *
 * Keep this file small: it should locate shims, not contain them.
 */
#ifndef DM_COMPAT_PLATFORM_H
#define DM_COMPAT_PLATFORM_H

#include <pthread_compat.h>
#include <TargetConditionals.h>

#include <sys/resource.h>
#include <unistd.h>

#include <mach/mach.h>
#include <pthread/qos.h>
#include <CommonCrypto/CommonDigest.h>
#include <fcntl_ext.h>
#include <rename_ext.h>
#include <sys/xattr.h>
#include <sigcontext_compat.h>
#include <sys/sysctl.h>
#include <pthread_tls_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * BSD/macOS helper that Bionic lacks: the number of file descriptors the
 * process may open. Upstream uses it to bound a diagnostic scan of open
 * descriptors (hle_fdstat.c). Derive it from RLIMIT_NOFILE rather than
 * guessing, and clamp the soft limit to what the hard limit allows.
 */
#ifndef DM_COMPAT_HAVE_GETDTABLESIZE
static inline int getdtablesize(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0) return 256;
    if (rl.rlim_cur == RLIM_INFINITY) return 65536;
    if (rl.rlim_cur > 0x7fffffff) return 0x7fffffff;
    return (int)rl.rlim_cur;
}
#endif

/* Darwin stat/time names used by the migrated D2R modules. */
#ifndef st_mtimespec
#define st_mtimespec st_mtim
#endif

/* Darwin's monotonic nanosecond clock spelling. Android CLOCK_MONOTONIC is
 * the corresponding Linux clock; the value is only used for elapsed deltas. */
#ifndef CLOCK_UPTIME_RAW
#define CLOCK_UPTIME_RAW CLOCK_MONOTONIC
#endif

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_PLATFORM_H */
