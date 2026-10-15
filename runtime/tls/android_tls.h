#ifndef DM_ANDROID_TLS_H
#define DM_ANDROID_TLS_H

#include <stdint.h>

enum dm_android_tls_slot {
    DM_TLS_K18 = 0,
    DM_TLS_KTLS = 1,
    DM_TLS_KTLR = 2,
    DM_TLS_KGATE = 3,
    DM_TLS_KRES0 = 4,
    DM_TLS_KRES1 = 5,
    DM_TLS_KASPECT = 6,
    DM_TLS_KASPECTUI = 7,
    DM_TLS_SLOT_COUNT = 8,
};

uint64_t dm_android_tls_get(unsigned slot);
void dm_android_tls_set(unsigned slot, uint64_t value);
uint64_t *dm_android_tls_base(void);
int dm_android_tls_self_test(void);

#endif
