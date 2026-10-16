#include "dm.h"
#include "front.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

/* Android bootstrap-only implementations. They are intentionally named and
 * logged so a synthetic boot can never be mistaken for D2R gameplay support. */
static void boot_stub(const char *n) { LOG("BOOT_STUB %s", n); }
void front_run(void (*guest)(void)) { boot_stub("front_run"); if (guest) guest(); }
uint64_t front_game_res(int docked) { (void)docked; return (900ull << 32) | 1600; }
int front_text_input(const char *a,const char*b,const char*c,const char*d,int e,char*f,int g){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; return -1;}
void dm_ui_text_entered(const char *s) { (void)s; }
void dm_prof_mark(const char *s) { if (s) LOG("BOOT_PROF %s", s); }
void dm_prof_add(int a, uint64_t b, uint64_t c) {(void)a;(void)b;(void)c;}
uint64_t dm_prof_ribbon_ns(void) { return 0; }
void probe_install(void) { boot_stub("probe_install"); }
void ticks_install(void) { boot_stub("ticks_install"); }
void inflate_bench_maybe(void) {}
void crash_sample_self(void) {}
void dm_padmode_boot(void) { boot_stub("dm_padmode_boot"); }
void dm_nvn_init(void) { boot_stub("dm_nvn_init"); }
void uimsg_subscribe(void (*fn)(const char*,const char*,const char*)) {(void)fn;}
void dm_ui(void) {}
void dm_hud_stack_msg(const char *a,const char*b,const char*c){(void)a;(void)b;(void)c;}
void dm_tc_init(void) {}
void dm_tc_set_joy_block(int x){(void)x;}
void dm_tc_enable_targets(int x){(void)x;}
void dm_tc_set_panel_state(int x){(void)x;}
void dm_tc_navigation(const char *a,const char*b,const char*c){(void)a;(void)b;(void)c;}
int dm_ui_panel_state(void){return 0;}
uint64_t dm_game_res(int docked){return front_game_res(docked);}
uint64_t dm_nvn_frame_count(void){return 0;}
uint64_t dm_nvn_sub_cpu_ns(void){return 0;}
