#include "android_tls.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct dm_android_tls_state { uint64_t slot[DM_TLS_SLOT_COUNT]; };
static pthread_key_t g_tls_key;
static pthread_once_t g_tls_once = PTHREAD_ONCE_INIT;

static void tls_destroy(void *p) { free(p); }
static void tls_init_key(void) {
    if (pthread_key_create(&g_tls_key, tls_destroy) != 0) abort();
}
static struct dm_android_tls_state *tls_state(void) {
    pthread_once(&g_tls_once, tls_init_key);
    struct dm_android_tls_state *s = pthread_getspecific(g_tls_key);
    if (!s) {
        s = calloc(1, sizeof *s);
        if (!s || pthread_setspecific(g_tls_key, s) != 0) abort();
    }
    return s;
}

uint64_t dm_android_tls_get(unsigned slot) {
    return slot < DM_TLS_SLOT_COUNT ? tls_state()->slot[slot] : 0;
}
void dm_android_tls_set(unsigned slot, uint64_t value) {
    if (slot < DM_TLS_SLOT_COUNT) tls_state()->slot[slot] = value;
}
uint64_t *dm_android_tls_base(void) { return tls_state()->slot; }

struct tls_thread_arg { uint64_t k18, ktlr; int ok; };
static void *tls_thread(void *arg) {
    struct tls_thread_arg *a = arg;
    dm_android_tls_set(DM_TLS_K18, a->k18);
    dm_android_tls_set(DM_TLS_KTLR, a->ktlr);
    a->ok = dm_android_tls_get(DM_TLS_K18) == a->k18 &&
            dm_android_tls_get(DM_TLS_KTLR) == a->ktlr;
    return NULL;
}

int dm_android_tls_self_test(void) {
    const uint64_t main_k18 = 0x1111222233334444ull;
    const uint64_t main_ktlr = 0xaaaabbbbccccddddull;
    dm_android_tls_set(DM_TLS_K18, main_k18);
    dm_android_tls_set(DM_TLS_KTLR, main_ktlr);
    if (dm_android_tls_get(DM_TLS_K18) != main_k18 ||
        dm_android_tls_get(DM_TLS_KTLR) != main_ktlr) return 0;

    struct tls_thread_arg child = {0x5555666677778888ull, 0x9999aaaabbbbccccull, 0};
    pthread_t t;
    if (pthread_create(&t, NULL, tls_thread, &child) != 0) return 0;
    pthread_join(t, NULL);
    return child.ok && dm_android_tls_get(DM_TLS_K18) == main_k18 &&
           dm_android_tls_get(DM_TLS_KTLR) == main_ktlr;
}
