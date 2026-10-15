/*
 * mach/thread_policy.h — Android/POSIX shim for Darwin real-time thread policy.
 *
 * Android has SCHED_FIFO under a different API and a very different permission
 * model (unprivileged apps cannot use it for arbitrary threads). Trying to
 * emulate THREAD_TIME_CONSTRAINT_POLICY would give the framework a policy it
 * cannot honour, so set/get report "no policy in effect". Upstream already
 * treats "not in effect" as a valid state and logs it, so behavior stays
 * honest: pacing falls back to the normal scheduler.
 */
#ifndef DM_COMPAT_MACH_THREAD_POLICY_H
#define DM_COMPAT_MACH_THREAD_POLICY_H

#include <stdint.h>
#include <pthread.h>
#include "mach/mach.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int thread_policy_set(mach_port_t thread, int flavor,
                                    void *policy, mach_msg_type_number_t count) {
    (void)thread; (void)flavor; (void)policy; (void)count;
    return KERN_FAILURE;
}

static inline int thread_policy_get(mach_port_t thread, int flavor,
                                    void *policy, mach_msg_type_number_t *count,
                                    boolean_t *is_default) {
    (void)thread; (void)flavor; (void)policy; (void)count;
    if (is_default) *is_default = TRUE;
    return KERN_FAILURE;
}

static inline mach_port_t pthread_mach_thread_np(pthread_t t) {
    return (mach_port_t)(uintptr_t)t;
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_MACH_THREAD_POLICY_H */
