// Android host-side defaults, consumed by the generic HLE layer.
//
// Split in two on purpose:
//
//   * Symbols the D2R adapter also defines are declared WEAK here.  They are the
//     seat the adapter takes over when it is linked in: with no adapter the
//     runtime still boots with these inert values, and with one the adapter's
//     strong definitions win.  This is why the two can be built independently
//     (adapter -> runtime) and linked in either combination.
//   * Symbols that are genuinely runtime-side (no adapter counterpart) are
//     strong definitions.
//
// Never move adapter-owned behaviour in here; it belongs in the adapter.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DM_WEAK __attribute__((weak))

// --- language / clock: the adapter owns these when it is present -----------
// dm_i18n.c and dm_custom.c in the adapter define both language entry points, so
// these are weak for the same reason as the seats below.
DM_WEAK int dm_sys_lang(void) {
    const char *e = getenv("DM_LANG");
    if (e && (!strcmp(e, "en") || !strcmp(e, "en-US"))) return 0;
    return 8;   // Simplified Chinese, the upstream non-Apple default
}
DM_WEAK int dm_custom_lang_explicit(void) { return -1; }   // language switch never written by the player
DM_WEAK uint64_t dm_server_tick(void) { return UINT64_MAX; }   // not in a run

// --- adapter-owned seats: inert here, overridden when the adapter is linked -
// The adapter provides dm_cursor_set* / dm_ui_touch* / dm_ui_* (dm_cursor.c,
// dm_uiscreen.c).  Keeping these weak means a runtime-only build still links.
DM_WEAK void dm_cursor_set(int x, int y) { (void)x; (void)y; }
DM_WEAK void dm_cursor_set_screen(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
DM_WEAK void dm_cursor_clear(void) {}
DM_WEAK int dm_cursor_pending(void) { return 0; }
DM_WEAK void dm_ui_touch(int phase, float nx, float ny) { (void)phase; (void)nx; (void)ny; }
DM_WEAK int dm_ui_touch_active(void) { return 0; }
DM_WEAK void dm_ui_rect(int *x, int *y, int *w, int *h) { if (x) *x = 0; if (y) *y = 0; if (w) *w = 1280; if (h) *h = 720; }
DM_WEAK float dm_ui_ny(float y) { return y; }
DM_WEAK float dm_ui_ny_back(float y) { return y; }
DM_WEAK int dm_ui_proj_dy(void) { return 0; }
