/* Adapter seam — inert defaults for the D2R adapter contract.
 *
 * The generic runtime must not contain D2R knowledge (project charter: the
 * runtime exposes stable interfaces, the adapter implements game-specific
 * behaviour).  The real implementations live in MrCKR/d2r-android-adapter.
 *
 * These definitions exist only so the APK can link and the guest can be
 * brought up before the adapter is wired in.  Every one of them is deliberately
 * inert: no touch injection, no world taps, no inventory actions.  When the
 * adapter is linked, its strong definitions must replace this file wholesale.
 *
 * Do not add gameplay behaviour here.
 */
#include <stdint.h>
#include <stddef.h>
#include "dm_cursor_item.h"

#define DM_WEAK __attribute__((weak))

/* --- receipt pumps (svc 0x81 / 0x82 handlers) --- */
DM_WEAK void c2s_deliver(uint64_t *regs) { (void)regs; }
DM_WEAK void c2s_deliver_sys(uint64_t *regs) { (void)regs; }

/* --- per-frame pumps --- */
DM_WEAK void dm_actions_pump(void) {}
DM_WEAK void dm_update_pump(void) {}
DM_WEAK void dm_world_pump(void) {}
DM_WEAK void dm_ui_mouse_pump(void) {}
DM_WEAK void dm_textinput_pump(void) {}
DM_WEAK void dm_ui_frontend_pump(void) {}

/* --- run state --- */
DM_WEAK int dm_ingame_now(void) { return 0; }
DM_WEAK int dm_loadscreen_up(void) { return 0; }
DM_WEAK void dm_hold_for_loadscreen(void) {}
DM_WEAK int dm_nvn_ruler_truth(void) { return 0; }
DM_WEAK void dm_gfx_suspend(int on) { (void)on; }

/* --- pacing / display --- */
DM_WEAK double mtl_display_refresh(void) { return 0.0; }
DM_WEAK int mtl_fps_target(void) { return 60; }
DM_WEAK void mtl_rt_force_full(int n) { (void)n; }
DM_WEAK double dm_pace_hz(double screen_hz, double floor_hz) { return screen_hz > 0 ? screen_hz : floor_hz; }

/* --- input mode / filters --- */
DM_WEAK int dm_padmode_current(void) { return 0; }
DM_WEAK int dm_padmode_input_gate(uint64_t *buttons) { (void)buttons; return 0; }
DM_WEAK uint64_t dm_padmode_page_filter(uint64_t buttons) { return buttons; }
DM_WEAK uint64_t dm_runeword_pad_filter(uint64_t buttons) { return buttons; }
DM_WEAK uint64_t dm_swapskill_filter(uint64_t buttons) { return buttons; }
DM_WEAK uint64_t dm_ui_auto_buttons(void) { return 0; }
DM_WEAK uint64_t dm_ui_auto_release(void) { return 0; }
DM_WEAK uint64_t dm_ui_alone_take(uint64_t others) { (void)others; return 0; }
DM_WEAK void dm_ui_map_full(int on) { (void)on; }

/* --- cursor / pointer routing --- */
DM_WEAK void dm_cursor_apply(uint64_t *buttons, int32_t *lx, int32_t *ly) { (void)buttons; (void)lx; (void)ly; }
DM_WEAK int dm_cursor_item(dm_cursor_item_info *out) { (void)out; return 0; }
DM_WEAK int dm_held_size(int *w, int *h) { (void)w; (void)h; return 0; }
DM_WEAK int dm_held_pending(void) { return 0; }
DM_WEAK int dm_item_targeting(void) { return 0; }
DM_WEAK int dm_vendor_repair_mode(void) { return 0; }
DM_WEAK void dm_ui_ptr(int kind, float nx, float ny, int arg) { (void)kind; (void)nx; (void)ny; (void)arg; }
DM_WEAK void dm_ui_send_queue(const char *spec) { (void)spec; }
DM_WEAK void dm_ui_tab_go(void) {}
DM_WEAK uint64_t dm_ui_taps_base(void) { return 0; }
DM_WEAK unsigned dm_ui_item_query(float nx, float ny) { (void)nx; (void)ny; return 0; }
DM_WEAK int dm_ui_item_result(unsigned seq) { (void)seq; return -1; }
DM_WEAK unsigned dm_ui_drop_query(float nx, float ny_lifted, float ny_finger) { (void)nx; (void)ny_lifted; (void)ny_finger; return 0; }
DM_WEAK int dm_ui_drop_point(float *nx, float *ny) { (void)nx; (void)ny; return 0; }
DM_WEAK void dm_ui_drag_begin(int kind, float ox, float oy, float tol_ny) { (void)kind; (void)ox; (void)oy; (void)tol_ny; }
DM_WEAK void dm_ui_drag_move(float x, float y) { (void)x; (void)y; }
DM_WEAK void dm_ui_drag_end(void) {}
DM_WEAK int dm_ui_bind_active(void) { return 0; }
DM_WEAK int dm_ui_bind_skill_hit(float *nx, float *ny) { (void)nx; (void)ny; return 0; }

/* --- world interaction --- */
DM_WEAK unsigned dm_world_tap(float nx, float ny) { (void)nx; (void)ny; return 0; }
DM_WEAK int dm_world_tap_result(unsigned seq) { (void)seq; return -1; }

/* --- inventory / shop actions --- */
DM_WEAK void dm_shop_buy(void) {}
DM_WEAK void dm_shop_sell_cursor(void) {}
DM_WEAK void dm_skill_add_selected(void) {}
DM_WEAK void dm_stat_add(int stat, int n) { (void)stat; (void)n; }
DM_WEAK void dm_belt_put(uint32_t item_id) { (void)item_id; }
DM_WEAK void dm_belt_expand(int on) { (void)on; }
DM_WEAK void dm_item_compare(int on) { (void)on; }
DM_WEAK void dm_item_use(uint32_t item_id) { (void)item_id; }
DM_WEAK void dm_item_identify(uint32_t target_id) { (void)target_id; }
DM_WEAK void dm_inv_sort(int page) { (void)page; }
DM_WEAK void dm_name_random(void) {}
DM_WEAK void dm_num_set(int n) { (void)n; }
DM_WEAK void dm_held_stow(void) {}
DM_WEAK int dm_held_stow_result(void) { return 0; }
DM_WEAK void dm_stash_take(int count) { (void)count; }
DM_WEAK void dm_stash_take_id(uint32_t item_id, int count) { (void)item_id; (void)count; }

/* --- UI screen rectangle --- */
DM_WEAK void dm_uiscreen_enable(int mode) { (void)mode; }
DM_WEAK void dm_uiscreen_res_reread(void) {}

/* --- svc handlers owned by the adapter --- */
DM_WEAK void svc_ground_asset(uint64_t *regs) { (void)regs; }
DM_WEAK void svc_drop_sound(uint64_t *regs) { (void)regs; }
DM_WEAK void svc_rift_exit(uint64_t *regs) { (void)regs; }
DM_WEAK void svc_save_act(uint64_t *regs) { (void)regs; }
DM_WEAK void svc_merc_slot(uint64_t *regs) { (void)regs; }
DM_WEAK void svc_merc_slot_cl(uint64_t *regs) { (void)regs; }
DM_WEAK void svc_join_spawn(uint64_t *regs, int corpse) { (void)regs; (void)corpse; }

/* --- scripted test actions (DM_UI_TAPS etc.): inert --- */
DM_WEAK void dm_test_belt_first_potion(void) {}
DM_WEAK void dm_test_hold_first_sellable(void) {}
DM_WEAK void dm_test_swap_held(void) {}
DM_WEAK void dm_test_sell_first(void) {}
DM_WEAK void dm_test_equip(int what) { (void)what; }
DM_WEAK void dm_test_identify(int what) { (void)what; }
DM_WEAK void dm_test_stash_take(int n) { (void)n; }
DM_WEAK void dm_test_use_first(int kind) { (void)kind; }
DM_WEAK void dm_test_hover_at(float nx, float ny) { (void)nx; (void)ny; }
