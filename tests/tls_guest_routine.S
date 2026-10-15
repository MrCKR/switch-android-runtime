/*
 * The guest routine follows the AArch64 ABI and restores x19-x24 before
 * returning. The rewrite blocks themselves preserve every register they
 * are allowed to touch; this test checks the observed slot values and the
 * ordinary live registers after the rewritten sequence.
 */
    .text
    .globl guest_tls_routine
    .p2align 2
guest_tls_routine:
    // Provide the stack scratch area that a real guest function has. The
    // rewrite blocks use the caller's red zone convention; this synthetic
    // routine must not make that area overlap its C caller's frame.
    sub  sp, sp, #1024
    stp  x19, x20, [sp, #448]
    stp  x21, x22, [sp, #464]
    stp  x23, x24, [sp, #480]
    str  x30, [sp, #496]
    mov  x9, x0

    movz x19, #0x1234                   // value to store into KTLS
    movk x19, #0xabcd, lsl #16
    msr  tpidr_el0, x19                 // [rewrite site: tpidr_w]

    mrs  x20, tpidr_el0                 // [rewrite site: tpidr_r]

    movz x21, #0x0011                   // sentinels: ordinary live registers
    movz x22, #0x0022
    movz x23, #0x0033

    mrs  x24, tpidrro_el0               // [rewrite site: tpidrro_r]

    str  x20, [x9, #0]
    str  x21, [x9, #8]
    str  x22, [x9, #16]
    str  x23, [x9, #24]
    str  x24, [x9, #32]
    str  x19, [x9, #40]
    mov  x0, #0
    /* The synthetic routine follows the AArch64 ABI and restores x19-x24
     * before returning, so the C harness's locals cannot be overwritten by
     * guest-only sentinel values. */
    ldp  x19, x20, [sp, #448]
    ldp  x21, x22, [sp, #464]
    ldp  x23, x24, [sp, #480]
    ldr  x30, [sp, #496]
    add  sp, sp, #1024
    ret
