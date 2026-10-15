/*
 * pthread extensions — Android/Bionic shim.
 *
 * Apple exposes these helpers; Bionic does not, so provide equivalents that
 * return the same information through the public POSIX / /proc interface.
 */
#ifndef DM_COMPAT_PTHREAD_EXT_H
#define DM_COMPAT_PTHREAD_EXT_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mach thread port for a pthread. Upstream uses it only as an opaque handle to
 * identify the thread, so the pthread_t itself serves that purpose.
 */
static inline uintptr_t pthread_mach_thread_np(pthread_t t) {
    return (uintptr_t)t;
}

/*
 * Base address of a thread's stack. Read from /proc/self/maps: the lowest
 * mapped region whose bounds contain the current stack pointer is the stack.
 */
static inline void *pthread_get_stackaddr_np(pthread_t t) {
    if (!pthread_equal(t, pthread_self())) return NULL;

    char buf[512];
    uintptr_t sp = (uintptr_t)__builtin_frame_address(0);
    uintptr_t best_base = 0, best_end = 0;
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return NULL;

    while (fgets(buf, sizeof(buf), f)) {
        uintptr_t lo = 0, hi = 0;
        if (sscanf(buf, "%lx-%lx", &lo, &hi) != 2) continue;
        if (sp >= lo && sp < hi) {
            if (!best_end || (hi - lo) < (best_end - best_base)) {
                best_base = lo;
                best_end = hi;
            }
        }
    }
    fclose(f);
    return best_end ? (void *)best_base : NULL;
}

/* Stack size for the current thread, derived from the same mapping. */
static inline size_t pthread_get_stacksize_np(pthread_t t) {
    pthread_attr_t attr;
    size_t sz = 0;
    if (pthread_getattr_np(t, &attr) == 0) {
        pthread_attr_getstacksize(&attr, &sz);
        pthread_attr_destroy(&attr);
    }
    return sz;
}

/* Nanosecond monotonic clock; Apple spells it as a one-call helper. */
static inline uint64_t clock_gettime_nsec_np(clockid_t clk) {
    struct timespec ts;
    if (clock_gettime(clk, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_PTHREAD_EXT_H */
