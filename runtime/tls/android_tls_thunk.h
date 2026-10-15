#ifndef DM_ANDROID_TLS_THUNK_H
#define DM_ANDROID_TLS_THUNK_H
#include <stdint.h>
/* Generic ABI for the first Android rewrite prototype.
 * x0 = slot index, x1 = value for write; x0 = returned value for read. */
uint64_t dm_android_tls_thunk_get(uint64_t slot);
void dm_android_tls_thunk_set(uint64_t slot, uint64_t value);
#endif
