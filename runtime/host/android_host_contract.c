// Host-side symbols the D2R adapter references but the runtime did not define.
//
// These turned up the honest way: linking the adapter archive into the library
// succeeded, because a shared object is allowed to carry unresolved symbols —
// but Android's dlopen is strict and refused to load it:
//
//     UnsatisfiedLinkError: cannot locate symbol "g_dm_frz_want"
//
// The earlier "no missing symbols" result was measured with a linker that does
// not enforce resolution.  Everything in here is runtime-side by the contract in
// d2r-android-adapter/docs/CONTRACT.md; none of it is D2R logic.
//
// Two groups:
//
//   * globals the adapter publishes through (g_dm_frz_want, g_dm_seek_tp*, ...)
//     — inert storage the runtime owns, so they start zeroed.  The adapter's
//     dm_cursor/dm_townwp write them.
//   * clock and keyboard helpers the HLE layer answers guest calls with
//     (dm_user_clock, dm_3d_quality_mode_*, dm_fs_d2s_wseq, ...).
//
// The ten symbols CONTRACT.md lists as the adapter's migration debt (dm_cur_tick,
// dm_diag_*, dm_freeze_frame, dm_request_save, dm_quiet_difficulty_msg,
// dm_fs_commit_count, dm_ingame_now, dm_loadscreen_up) are deliberately NOT
// implemented here: they belong in the adapter and moving them is that repo's
// task.  They are weak so the adapter overrides them once it does.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "dm.h"

#define DM_WEAK __attribute__((weak))

// --- globals the adapter publishes through ---------------------------------
// Defined (not merely declared) even in a runtime-only build: the contract
// requires them to exist as NULL/zero, because the adapter registers into the
// runtime's save path through them.
int g_dm_frz_want = 0;
int g_dm_seek_tp = 0;
int g_dm_seek_tp_x = 0;
int g_dm_seek_tp_y = 0;
// g_dm_l_presses is defined by hle_hid.c (as _Atomic) and g_swkbd_numeric_max by
// hle_swkbd.c, so neither is repeated here.  dm_cur_tick is below because
// touch_mac.c, which owns it on Apple, is not part of the Android build.

// --- clock ------------------------------------------------------------------
// The guest has no real-time clock of its own; it asks the host.  Monotonic
// nanoseconds since the first call, so the guest never sees time going backwards.
static uint64_t host_epoch_ns(void) {
    static uint64_t base = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    if (base == 0) base = now;
    return now - base;
}

DM_WEAK int64_t dm_user_clock(void) { return (int64_t)host_epoch_ns(); }
DM_WEAK int dm_user_clock_fixed(void) { return 0; }      // clock is not frozen
DM_WEAK void dm_test_clock_shift(int64_t d) { (void)d; } // developer knob, unused

// --- guest-visible host state ----------------------------------------------
DM_WEAK int dm_3d_quality_mode_get(void) { return 1; }   // stereo 3D off
DM_WEAK void dm_3d_quality_mode_set(int mode) { (void)mode; }

// Frames since the guest started, used by the guest's own pacing checks.
// touch_mac.c defines this on Apple; the Android build does not compile it, so
// the runtime has to own a counter here.
DM_WEAK uint32_t dm_cur_tick(void) {
    static uint32_t t = 0;
    return ++t;
}

// The guest asks whether a save is being written to the device's filesystem
// rather than a container; on Android the answer is always no.
DM_WEAK uint64_t dm_fs_d2s_wseq(void) { return 0; }
DM_WEAK int64_t dm_user_clock_raw_ns(void) { return (int64_t)host_epoch_ns(); }

// --- diagnostics ------------------------------------------------------------
// These publish to the adapter's overlay.  With no adapter there is nothing to
// show, so they report "unknown" rather than inventing values.
DM_WEAK int dm_diag_difficulty(void) { return -1; }
DM_WEAK uint64_t dm_diag_srv_item(uint32_t id) { (void)id; return 0; }
DM_WEAK int dm_diag_srv_merc(uint32_t id, int *hp, int *mx, int *healing) {
    (void)id; (void)hp; (void)mx; (void)healing; return 0;
}

// --- UI messages -------------------------------------------------------------
// The adapter's overlay subscribes via uimsg_subscribe.  With no adapter nobody
// is listening, so emitting is a no-op.  Note the signature: three strings, not
// a payload pointer (CONTRACT.md calls this out because the upstream call sites
// make it easy to get wrong).
DM_WEAK void uimsg_emit(const char *kind, const char *text, const char *detail) {
    (void)kind; (void)text; (void)detail;
}

// --- Darwin syscall shim ----------------------------------------------------
// thread_selfcounts comes from libpthread on Darwin.  Bionic has no equivalent,
// and only dm_prof.c calls it for statistics, so report "unsupported" instead of
// pretending to have numbers.
DM_WEAK int thread_selfcounts(int type, void *buf, size_t nbytes) {
    (void)type; (void)buf; (void)nbytes;
    return -1;
}

// --- adapter migration debt --------------------------------------------------
// CONTRACT.md lists these ten as belonging to the adapter, currently stranded in
// the runtime's diag.c.  The adapter declares and calls them but does not define
// them, so without definitions nothing links.  Weak definitions live here as the
// seat; the adapter takes them over when the migration happens.
//
//   dm_freeze_frame          held in dm_cursor.c, defined in the old diag.c
//   dm_quiet_difficulty_msg  called from dm_rift.c
//   dm_request_save          called from dm_pouch.c / dm_pages.c
//   dm_seq_next              called from the HLE fs layer
//   dm_fs_commit_count       companion to the save path
//   dm_ingame_now            HLE queries this to tell menus from gameplay
//   dm_loadscreen_up         HLE queries this while the guest loads
//
// Each is inert: no save is triggered, no notice is suppressed, nothing is
// frozen.  That is the honest answer while the gameplay logic that would decide
// these lives in the adapter.
DM_WEAK void dm_freeze_frame(int want) { (void)want; }
DM_WEAK void dm_quiet_difficulty_msg(int on) { (void)on; }
DM_WEAK int dm_request_save(void) { return 0; }
DM_WEAK uint64_t dm_seq_next(void) { return 0; }
DM_WEAK uint64_t dm_fs_commit_count(void) { return 0; }
DM_WEAK int dm_ingame_now(void) { return 0; }
DM_WEAK int dm_loadscreen_up(void) { return 0; }

// --- macOS-only -------------------------------------------------------------
// CONTRACT.md marks dm_gfx_shot_at as Apple-only (it captures a frame from the
// Metal path).  The Android backend does not provide screenshots this way, but
// the symbol has to resolve for the archive to load.  Note the two arguments:
// dm.h declares (long, const char *), not (const char *) alone.
DM_WEAK void dm_gfx_shot_at(long k, const char *path) { (void)k; (void)path; }
