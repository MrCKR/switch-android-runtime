// Per-thread record of the host call currently being executed.
//
// Purpose: when the guest stops making progress the log just ends, and there is
// no way to tell whether it died, blocked in the host, or is spinning in its own
// code.  A single thread-local pointer answers that: the sampler prints it, so
// the last line before the silence names the call it was inside.
//
// Deliberately not a call stack.  Nesting happens (an HLE call that calls
// another), and keeping the previous value on the C stack costs nothing and
// still lets the outer call restore its own name on return.  Only the innermost
// name is visible, which is what is wanted.
#include <string.h>

#include "dm.h"
#include "hle.h"

static _Thread_local const char *g_trace;

const char *dm_hle_trace_enter(const char *why) {
    const char *prev = g_trace;
    g_trace = why;
    return prev;
}

const char *dm_hle_trace_leave(const char *prev) {
    g_trace = prev;
    return prev;
}

const char *dm_hle_trace_get(void) {
    return g_trace ? g_trace : "(none)";
}
