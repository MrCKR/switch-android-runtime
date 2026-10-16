/* Load a real, user-produced patched D2R module without entering it.
 *
 * This deliberately stops after map/parse/gate registration/relocation. It
 * proves the runtime accepts the real artifact format and performs the work
 * that is independent of sdk/RomFS/NVN. It does not claim gameplay support.
 */
#include "dm.h"
#include "hle.h"
#include <stdio.h>
#include <string.h>

uint64_t unimpl_stubs[4096];
struct dm_flags g_flag;
uint64_t g_t0_ns;
uint64_t now_ns(void) { return 1; }
uint64_t guest_ns(void) { return 1; }
void hlecount_seal(void) {}
void *hlecount_wrap(const char *a, const char *b, void *p) { (void)a; (void)b; return p; }
uint64_t svc_dispatch(int n, uint64_t *r, uint64_t o) { (void)n; (void)r; (void)o; return 0; }
uint64_t dm_server_tick(void) { return UINT64_MAX; }
void dm_guest_clock_advanced(void) {}
void guest_clock_freeze(int on) { (void)on; }
void guest_clock_freeze_for(int r, int on) { (void)r; (void)on; }
int dm_detclock_enabled(void) { return 0; }
void dm_guest_frame_tick(void) {}
void dm_gfx_suspend(int on) { (void)on; }
void dm_gfx_shot_at(long k, const char *p) { (void)k; (void)p; }
void dm_shot_written(const char *p, int ok) { (void)p; (void)ok; }
void dm_host_shutdown(const char *p) { (void)p; }
void dm_touch_set(const void *p, int n) { (void)p; (void)n; }
void front_save_failed(int e) { (void)e; }
void *objmap_get(const void *p) { (void)p; return NULL; }
void objmap_put(const void *a, void *b) { (void)a; (void)b; }
void *objmap_del(const void *p) { (void)p; return NULL; }
void hle_signal_system_event(void *p) { (void)p; }
void hle_bind_system_event(void *p, int a) { (void)p; (void)a; }
void dm_guest_write_prepare(void *p, size_t n) { (void)p; (void)n; }
void dm_guest_write_prep_register(void (*p)(void *, size_t)) { (void)p; }
void dm_fault_hook_register(int (*p)(void *, int, void *)) { (void)p; }
void dm_res_report_register(void (*p)(uint32_t, const char *)) { (void)p; }
void (*dm_res_report_cb(void))(uint32_t, const char *) { return NULL; }
void dm_backtrace(uint64_t x) { (void)x; }
void crash_watch_progress(uint32_t a, const char *b) { (void)a; (void)b; }
void crash_watch_frame(void) {}
void crash_watch_pregame_done(void) {}
uint64_t crash_watch_progress_count(void) { return 0; }
void crash_dump_now(const char *p) { (void)p; }

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: real_module_load_test <staging-dir>\n"); return 2; }
    const char *names[] = { "main" };
    load_modules(argv[1], names, 1);
    module_t *m = &g_mod[0];
    printf("real module metadata accepted\n");
    printf("module=%s build_artifact_size=%#zx base=%p patch_off=%#llx text_end=%#llx gates=loaded\n",
           m->name, m->size, (void *)m->base,
           (unsigned long long)m->patch_off, (unsigned long long)m->text_end);
    printf("real module load: PASS\n");
    return 0;
}
