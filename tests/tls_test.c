#include "../runtime/tls/android_tls.h"
#include <stdio.h>
int main(void) {
    int ok = dm_android_tls_self_test();
    printf("tls test: %s\n", ok ? "PASS" : "FAIL");
    for (unsigned i = 0; i < DM_TLS_SLOT_COUNT; i++)
        dm_android_tls_set(i, 0x1000000000000000ull + i);
    for (unsigned i = 0; i < DM_TLS_SLOT_COUNT; i++)
        if (dm_android_tls_get(i) != 0x1000000000000000ull + i) ok = 0;
    printf("slots: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
