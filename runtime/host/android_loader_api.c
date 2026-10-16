/* Android guest loader API.
 *
 * The APK accepts a complete rewritten module set.  Game data remains user
 * supplied; this API only loads the staged rewritten images and their tables.
 */
#include "dm.h"
#include "hle.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <android/log.h>

#define TAG "SwitchRuntime"
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

struct dm_flags g_flag;
uint64_t g_t0_ns;
_Atomic int g_dm_frozen;
_Atomic int g_frz_by_panel;
_Atomic int g_bgsave;

/* Generic host implementations are linked from hle_*.c below. */

/* HLE/loader helpers are supplied by the full generic host objects. */

void dm_android_start_guest(void);
void guest_thread_init(void);

int dm_android_load_module_dir(const char *dir) {
    if (!dir || !*dir) return -1;
    /* The 1.0.34 program load chain is rtld, main, sdk, subsdk0.  rtld is
     * metadata-only here; the executable modules are loaded in dependency
     * order so cross-module symbol resolution sees the complete set. */
    const char *names[] = {"main", "sdk", "subsdk0"};
    ALOG("guest staging accepted: %s", dir);
    load_modules(dir, names, 3);
    for (int i = 0; i < 3; ++i) {
        module_t *m = &g_mod[i];
        ALOG("module %s mapped: base=%p size=%#zx patch_off=%#llx text_end=%#llx",
             m->name, (void *)m->base, m->size,
             (unsigned long long)m->patch_off,
             (unsigned long long)m->text_end);
    }
    ALOG("guest modules relocated: count=%d", g_nmod);
    ALOG("guest gates registered");
    ALOG("guest entry pending: HLE/NVN bootstrap");
    dm_android_start_guest();
    return 0;
}

/* Minimal real guest bootstrap entry.  This deliberately uses the already
 * loaded modules and reports the first failing initialization stage. */
static void *guest_req(const char *name) {
    void *p = guest_sym(name);
    if (!p) { ALOG("guest bootstrap missing symbol: %s", name); return NULL; }
    return p;
}

static void *android_guest_bootstrap(void *unused) {
    (void)unused;
    ALOG("guest entry: bootstrap thread starting");
    ALOG("guest entry: before thread environment");
    guest_thread_init();
    ALOG("guest entry: thread environment ready");
    /* svc_thread_main_init is a host function, not a guest export.  The
     * Android bootstrap currently uses the conventional main-thread handle. */
    uint32_t hmain = 0x1001;
    void (*fn0)(void);
    void (*fn2)(uint64_t, uint64_t);
    fn0 = (void (*)(void))guest_req("_init_libc0");
    if (fn0) { ALOG("guest entry: _init_libc0"); fn0(); }
    fn2 = (void (*)(uint64_t, uint64_t))guest_req("nnosInitialize");
    if (fn2) { ALOG("guest entry: nnosInitialize"); fn2(hmain, 0); }
    fn0 = (void (*)(void))guest_req("_init_libc1");
    if (fn0) { ALOG("guest entry: _init_libc1"); fn0(); }
    fn0 = (void (*)(void))guest_req("nninitInitializeSdkModule");
    if (fn0) { ALOG("guest entry: nninitInitializeSdkModule"); fn0(); }
    fn0 = (void (*)(void))guest_req("nninitStartup");
    if (fn0) { ALOG("guest entry: nninitStartup"); fn0(); }
    fn0 = (void (*)(void))guest_req("_init_libc2");
    if (fn0) { ALOG("guest entry: _init_libc2"); fn0(); }
    fn0 = (void (*)(void))guest_req("nnMain");
    if (fn0) { ALOG("guest entry: nnMain"); fn0(); }
    ALOG("guest entry: nnMain returned");
    return NULL;
}

void dm_android_start_guest(void) {
    static int started;
    if (__atomic_exchange_n(&started, 1, __ATOMIC_ACQ_REL)) return;
    pthread_t t;
    if (pthread_create(&t, NULL, (void *(*)(void *))android_guest_bootstrap, NULL) == 0) {
        pthread_detach(t);
        ALOG("guest entry: thread launched");
    } else {
        ALOG("guest entry: pthread_create failed");
    }
}

