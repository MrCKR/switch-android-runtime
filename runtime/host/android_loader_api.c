/* Android guest loader API.
 *
 * This first APK integration deliberately stops after mapping, dynamic parsing,
 * gate registration and relocation. It is the same generic loader used by the
 * real-artifact smoke test; no D2R module is entered here yet.
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

uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
uint64_t guest_ns(void) { return now_ns(); }

/* Generic gate/loader dependencies that belong to the later HLE slices. Keep
 * them explicit: a missing implementation must be visible at link time, not
 * hidden by a weak-symbol soup. */
uint64_t svc_dispatch(int n, uint64_t *regs, uint64_t orig) {
    (void)regs; (void)orig;
    ALOG("BOOT_STUB svc_dispatch num=%#x", n);
    return 0;
}
void hlecount_seal(void) {}
void *hlecount_wrap(const char *kind, const char *name, void *fn) {
    (void)kind; (void)name; return fn;
}
void dm_thread_returned(void) { ALOG("BOOT_STUB dm_thread_returned"); }

/* HLE/loader helpers referenced by the generic objects but not needed until
 * guest execution is enabled. */
void dm_guest_write_prepare(void *p, size_t n) { (void)p; (void)n; }
void dm_guest_write_prep_register(void (*fn)(void *, size_t)) { (void)fn; }
void dm_fault_hook_register(int (*fn)(void *, int, void *)) { (void)fn; }
void dm_res_report_register(void (*fn)(uint32_t, const char *)) { (void)fn; }
void (*dm_res_report_cb(void))(uint32_t, const char *) { return NULL; }
void dm_backtrace(uint64_t fp) { (void)fp; }
void crash_watch_progress(uint32_t tick, const char *stage) { (void)tick; (void)stage; }
void crash_watch_frame(void) {}
void crash_watch_pregame_done(void) {}
uint64_t crash_watch_progress_count(void) { return 0; }
void crash_dump_now(const char *why) { (void)why; }

int dm_android_load_module_dir(const char *dir) {
    if (!dir || !*dir) return -1;
    const char *names[] = {"main"};
    ALOG("guest metadata accepted: staging=%s", dir);
    load_modules(dir, names, 1);
    module_t *m = &g_mod[0];
    ALOG("module main mapped: base=%p size=%#zx patch_off=%#llx text_end=%#llx",
         (void *)m->base, m->size,
         (unsigned long long)m->patch_off,
         (unsigned long long)m->text_end);
    ALOG("relocations complete");
    ALOG("gates registered");
    ALOG("guest entry not entered (metadata smoke mode)");
    return 0;
}
