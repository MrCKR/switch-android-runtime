// Android host-side defaults that are not part of the D2R adapter contract.
// Language/tick/touch entry points consumed by the generic HLE layer.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int dm_sys_lang(void) {
    const char *e = getenv("DM_LANG");
    if (e && (!strcmp(e, "en") || !strcmp(e, "en-US"))) return 0;
    return 8;   // Simplified Chinese, the upstream non-Apple default
}
int dm_custom_lang_explicit(void) { return -1; }   // language switch never written by the player
uint64_t dm_server_tick(void) { return UINT64_MAX; }   // not in a run
void dm_cursor_set(int x, int y) { (void)x; (void)y; }
void dm_cursor_set_screen(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void dm_cursor_clear(void) {}
int dm_cursor_pending(void) { return 0; }
void dm_ui_touch(int phase, float nx, float ny) { (void)phase; (void)nx; (void)ny; }
int dm_ui_touch_active(void) { return 0; }
void dm_ui_rect(int *x, int *y, int *w, int *h) { if (x) *x = 0; if (y) *y = 0; if (w) *w = 1280; if (h) *h = 720; }
float dm_ui_ny(float y) { return y; }
float dm_ui_ny_back(float y) { return y; }
int dm_ui_proj_dy(void) { return 0; }
