/*
 * mach/mach_time.h — Android/POSIX shim for the Darwin monotonic clock.
 *
 * Upstream treats mach_absolute_time() as a monotonic tick source and converts
 * with mach_timebase_info. On Android CLOCK_MONOTONIC already reports
 * nanoseconds, so the timebase is the identity (numer = denom = 1) and every
 * call site that does value * numer / denom keeps working unchanged.
 */
#ifndef DM_COMPAT_MACH_TIME_H
#define DM_COMPAT_MACH_TIME_H

#include <stdint.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t numer;
    uint32_t denom;
} mach_timebase_info_data_t;

typedef uint64_t mach_timebase_info_t;

static inline uint64_t mach_absolute_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static inline int mach_timebase_info(mach_timebase_info_data_t *info) {
    if (info) { info->numer = 1; info->denom = 1; }
    return 0;
}

/* Absolute sleep on the same time base returned by mach_absolute_time(). */
static inline void mach_wait_until(uint64_t deadline) {
    struct timespec ts;
    ts.tv_sec  = (time_t)(deadline / 1000000000ull);
    ts.tv_nsec = (long)(deadline % 1000000000ull);
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR) {
    }
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_MACH_TIME_H */
