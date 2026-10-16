/*
 * Register-preserving trampolines for rewritten guest TLS access.
 *
 * Contract: x0 is the slot index on entry for the get trampoline; for the set
 * trampoline x1 additionally carries the value. On return x0 holds the result
 * of a get and is unspecified for a set. EVERY other architectural register is
 * preserved: x1-x18, x30, the SIMD/FP registers v0-v7, and nzcv/fpsr/fpcr.
 *
 * Why the contract is this strict: a rewritten site replaces a single guest
 * instruction (`mrs tN, tpidr_el0`, `msr tpidr_el0, tN`, or an x18 read) that
 * modified exactly one register and no flags. The replacement must therefore
 * leave the entire remaining machine state untouched, or the surrounding guest
 * code breaks. The rewrite blocks save and restore the scratch registers they
 * use, so the only register this code may consume is x0 as the return value.
 *
 * C functions may clobber x0-x18, v0-v7 and the flags, so all of them are
 * saved here rather than by every call site.
 */
    .text

    .macro TlsSaveFrame
    sub sp, sp, #320
    stp x1,  x2,  [sp, #0]
    stp x3,  x4,  [sp, #16]
    stp x5,  x6,  [sp, #32]
    stp x7,  x8,  [sp, #48]
    stp x9,  x10, [sp, #64]
    stp x11, x12, [sp, #80]
    stp x13, x14, [sp, #96]
    stp x15, x16, [sp, #112]
    stp x17, x18, [sp, #128]
    stp x30, xzr, [sp, #144]
    stp q0, q1, [sp, #160]
    stp q2, q3, [sp, #192]
    stp q4, q5, [sp, #224]
    stp q6, q7, [sp, #256]
    mrs x9, nzcv
    mrs x10, fpsr
    mrs x11, fpcr
    stp x9,  x10, [sp, #288]
    str x11, [sp, #304]
    .endm

    /* Restores everything the macro saved except x0, which carries the result. */
    .macro TlsRestoreFrame
    ldp x9,  x10, [sp, #288]
    ldr x11, [sp, #304]
    msr nzcv, x9
    msr fpsr, x10
    msr fpcr, x11
    ldp q0, q1, [sp, #160]
    ldp q2, q3, [sp, #192]
    ldp q4, q5, [sp, #224]
    ldp q6, q7, [sp, #256]
    ldp x1,  x2,  [sp, #0]
    ldp x3,  x4,  [sp, #16]
    ldp x5,  x6,  [sp, #32]
    ldp x7,  x8,  [sp, #48]
    ldp x9,  x10, [sp, #64]
    ldp x11, x12, [sp, #80]
    ldp x13, x14, [sp, #96]
    ldp x15, x16, [sp, #112]
    ldp x17, x18, [sp, #128]
    ldr x30, [sp, #144]
    add sp, sp, #320
    .endm

    .globl dm_tls_tramp_get
    .p2align 2
dm_tls_tramp_get:
    TlsSaveFrame
    bl dm_android_tls_thunk_get
    TlsRestoreFrame
    ret

    .globl dm_tls_tramp_set
    .p2align 2
dm_tls_tramp_set:
    TlsSaveFrame
    bl dm_android_tls_thunk_set
    TlsRestoreFrame
    ret

    /* Slot-base trampoline for the Androidized guest images.  Those images keep
     * upstream's whole slot sequence and replace only the "mrs tpidrro_el0", so
     * the surrounding code still holds live values in x0..x18 across the call.
     * The getter itself is a plain C function that clobbers x0-x18, so every
     * caller-saved register must be preserved here (same contract as above). */
    .globl dm_tls_tramp_base
    .p2align 2
dm_tls_tramp_base:
    TlsSaveFrame
    bl dm_guest_tls_base
    TlsRestoreFrame
    ret
