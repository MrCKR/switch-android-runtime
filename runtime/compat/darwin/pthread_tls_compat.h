#ifndef DM_COMPAT_PTHREAD_TLS_H
#define DM_COMPAT_PTHREAD_TLS_H

#include <pthread.h>
#include <stdint.h>

/*
 * The upstream source uses pthread_getspecific as a typed value, for example:
 *
 *     *(uint64_t *)pthread_getspecific(g_keygate)
 *
 * It cannot be a comma-expression around a void-returning rewritten TLS
 * trampoline: that preserves the guest-register ABI but is not a C value.
 * These functions keep Bionic's real pthread key semantics for the host-side
 * keys. Guest rewritten sysreg accesses continue to use the assembly
 * register-preserving trampolines and never come through this API.
 */
static inline void *dm_android_pthread_getspecific(pthread_key_t key) {
    return pthread_getspecific(key);
}
static inline int dm_android_pthread_setspecific(pthread_key_t key, const void *value) {
    return pthread_setspecific(key, value);
}

#endif
