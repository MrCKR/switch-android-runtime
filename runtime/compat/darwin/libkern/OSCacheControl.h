/*
 * libkern/OSCacheControl.h — Android/AArch64 shim.
 *
 * sys_icache_invalidate() and sys_dcache_flush() are real requirements on any
 * platform that writes code and then executes it (upstream patches guest code,
 * so this is on the hot path). AArch64 provides the exact equivalents through
 * the compiler builtin, which emits the correct cache maintenance instructions
 * for the target: this is a genuine one-to-one replacement, not a no-op.
 */
#ifndef DM_COMPAT_OSCACHECONTROL_H
#define DM_COMPAT_OSCACHECONTROL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Invalidate instruction cache for a range (after writing code). */
static inline void sys_icache_invalidate(void *start, size_t len) {
    __builtin___clear_cache((char *)start, (char *)start + len);
}

/* Flush the data cache for a range (before the code above reads it). */
static inline void sys_dcache_flush(void *start, size_t len) {
    __builtin___clear_cache((char *)start, (char *)start + len);
}

#ifdef __cplusplus
}
#endif

#endif /* DM_COMPAT_OSCACHECONTROL_H */
