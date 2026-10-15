/* Exercises the rewrite-facing thunk ABI, not just the internal bridge. */
#include "../runtime/tls/android_tls_thunk.h"
#include <stdio.h>

int main(void) {
    int ok = 1;
    for (unsigned i = 0; i < 8; i++) {
        uint64_t v = 0xd000000000000000ull + i;
        dm_android_tls_thunk_set(i, v);
        if (dm_android_tls_thunk_get(i) != v) { printf("slot %u mismatch\n", i); ok = 0; }
    }
    printf("tls thunk: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
