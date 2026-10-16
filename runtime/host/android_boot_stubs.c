#include "dm.h"
#include "front.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

/* Android bootstrap-only implementations.
 *
 * Two kinds of symbol live here, and the difference matters:
 *
 *   runtime-side (front_run, front_game_res, front_text_input, dm_prof_*):
 *     genuinely the runtime's, because they are the front end this APK ships.
 *     Strong definitions.
 *
 *   adapter-side seats (dm_ui, dm_padmode_boot, dm_hud_stack_msg, ...):
 *     the D2R adapter owns these.  Declared WEAK so that linking the adapter
 *     archive takes over without any build configuration, while a runtime-only
 *     build still links and boots.  Nothing D2R-specific is implemented here.
 */
#define DM_WEAK __attribute__((weak))

/* --- runtime-side (this APK's front end) --- */
static void boot_stub(const char *n) { LOG("BOOT_STUB %s", n); }
void front_run(void (*guest)(void)) { boot_stub("front_run"); if (guest) guest(); }
uint64_t front_game_res(int docked) { (void)docked; return (900ull << 32) | 1600; }
int front_text_input(const char *a,const char*b,const char*c,const char*d,int e,char*f,int g){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; return -1;}
void dm_prof_mark(const char *s) { if (s) LOG("BOOT_PROF %s", s); }
void dm_prof_add(int a, uint64_t b, uint64_t c) {(void)a;(void)b;(void)c;}
uint64_t dm_prof_ribbon_ns(void) { return 0; }
void probe_install(void) { boot_stub("probe_install"); }
void ticks_install(void) { boot_stub("ticks_install"); }
void inflate_bench_maybe(void) {}
void crash_sample_self(void) {}
void dm_nvn_init(void) { boot_stub("dm_nvn_init"); }
void dm_tc_init(void) {}
uint64_t dm_nvn_frame_count(void){return 0;}
uint64_t dm_nvn_sub_cpu_ns(void){return 0;}

/* --- adapter-side seats (weak: the adapter archive overrides them) --- */
DM_WEAK void dm_ui_text_entered(const char *s) { (void)s; }
DM_WEAK void dm_padmode_boot(void) { boot_stub("dm_padmode_boot"); }
DM_WEAK void uimsg_subscribe(void (*fn)(const char*,const char*,const char*)) {(void)fn;}
DM_WEAK void dm_ui(void) {}
DM_WEAK void dm_hud_stack_msg(const char *a,const char*b,const char*c){(void)a;(void)b;(void)c;}
DM_WEAK void dm_tc_set_joy_block(int x){(void)x;}
DM_WEAK void dm_tc_enable_targets(int x){(void)x;}
DM_WEAK void dm_tc_set_panel_state(int x){(void)x;}
DM_WEAK void dm_tc_navigation(const char *a,const char*b,const char*c){(void)a;(void)b;(void)c;}
DM_WEAK int dm_ui_panel_state(void){return 0;}
DM_WEAK uint64_t dm_game_res(int docked){return front_game_res(docked);}
