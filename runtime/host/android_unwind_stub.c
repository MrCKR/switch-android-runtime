// Minimal stand-ins for the C++ unwinder.
//
// runtime.cpp is compiled as C++ (it bridges to the Java/NativeActivity side),
// so the compiler emits calls into the Itanium unwinder for anything that can
// throw.  The NDK sysroot ships no libunwind — libc++_shared.so on this device
// does not export _Unwind_Resume either — and the build links with
// -unwindlib=none, so the symbol has to come from somewhere.
//
// Nothing in this runtime throws: the code is C-style throughout and the guest
// runs with exceptions disabled in its own ABI.  So the only honest behaviour is
// "this should never be reached".  Aborting loudly beats letting the unwinder
// walk into a frame that has no CFI and produce a garbage crash report.
#include <stdint.h>
#include <stdlib.h>

#include "dm.h"

__attribute__((noreturn, visibility("default")))
void _Unwind_Resume(void *exception) {
    (void)exception;
    LOG("unwinder reached: something in the C++ bridge threw, and this build has "
        "no unwinder.  Treat as a programming error.");
    abort();
}
