#include "android_tls_thunk.h"
#include "android_tls.h"

uint64_t dm_android_tls_thunk_get(uint64_t slot) {
    return dm_android_tls_get((unsigned)slot);
}

void dm_android_tls_thunk_set(uint64_t slot, uint64_t value) {
    dm_android_tls_set((unsigned)slot, value);
}
