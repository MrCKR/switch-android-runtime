#!/usr/bin/env python3
"""Android guest TLS instruction rewrite.

Replaces guest TLS access sites with calls into register-preserving
trampolines, because on Android neither TPIDR_EL0 nor TPIDRRO_EL0 can carry a
guest slot base (see docs/SPEC-tls-rewrite.md for the measurements).

Per site the emitted patch block is:

    get (mrs xN, tpidr_el0|tpidrro_el0):
        stur x0, [sp, #-16]      ; only when N != 0
        movz x0, #slot
        bl   dm_tls_tramp_get    ; fixup: bound at load time
        mov  xN, x0              ; only when N != 0
        ldur x0, [sp, #-16]
        b    site+4

    set (msr tpidr_el0, xN):
        stur x0, [sp, #-16]
        stur x1, [sp, #-32]      ; only when N != 1
        mov  x1, xN              ; only when N != 1  (value)
        movz x0, #slot
        bl   dm_tls_tramp_set
        ldur x1, [sp, #-32]
        ldur x0, [sp, #-16]
        b    site+4

The trampoline preserves every register except x0, so the block only has to
look after x0/x1 itself. The red zone is used for the scratch saves, matching
upstream's convention.

Outputs, alongside <name>.patched.bin:
  <name>.fixups.tsv  layout header + one row per site needing load-time binding
  <name>.sites.tsv   per-kind site counts and offsets, for auditing
"""
import os
import re
import struct
import sys

# ---- encoders -----------------------------------------------------------------
def B(frm, to):
    off = to - frm
    assert -(1 << 27) <= off < (1 << 27) and off % 4 == 0, f'branch out of range {frm:#x}->{to:#x}'
    return 0x14000000 | ((off >> 2) & 0x3FFFFFF)

def BL(frm, to_placeholder):
    """Emitted with a zero displacement; the real address is bound at load time."""
    assert to_placeholder == 0
    return 0x94000000

def MOVZ(reg, imm):
    assert 0 <= imm < (1 << 16), imm
    return 0xD2800000 | (imm << 5) | reg

def MOV_REG(dst, src):
    return 0xAA0003E0 | (src << 16) | dst

def ADDI(reg, src, imm):
    assert 0 <= imm <= 4095
    return 0x91000000 | (imm << 10) | (src << 5) | reg

def SUBI(reg, src, imm):
    assert 0 <= imm <= 4095
    return 0xD1000000 | (imm << 10) | (src << 5) | reg

def STUR(rt, rn, off):
    assert -256 <= off <= 255 and off % 8 == 0
    return 0xF8000000 | ((off & 0x1FF) << 12) | (rn << 5) | rt

def LDUR(rt, rn, off):
    assert -256 <= off <= 255 and off % 8 == 0
    return 0xF8400000 | ((off & 0x1FF) << 12) | (rn << 5) | rt

# ---- site decoding ------------------------------------------------------------
MRS_TPIDR = 0xD53BD040       # mrs xN, tpidr_el0
MSR_TPIDR = 0xD51BD040       # msr tpidr_el0, xN
MRS_TPIDRRO = 0xD53BD060     # mrs xN, tpidrro_el0

SLOT_BY_KIND = {'tpidr_r': 1, 'tpidrro_r': 2, 'tpidr_w': 1}   # KTLS, KTLR, KTLS


def scan_sites(image, text_end):
    """Find TLS access sites. x18 handling is intentionally not attempted here:
    x18 reads are rewritten per function shape upstream and need the same
    control-flow analysis, which belongs in the follow-up that operates on a
    real module. This scanner reports what it saw so the gap is visible."""
    sites = []
    for off in range(0, text_end, 4):
        w = struct.unpack_from('<I', image, off)[0]
        if w & 0xFFFFFFE0 == MRS_TPIDR:
            sites.append((off, 'tpidr_r', w & 31))
        elif w & 0xFFFFFFE0 == MSR_TPIDR:
            sites.append((off, 'tpidr_w', w & 31))
        elif w & 0xFFFFFFE0 == MRS_TPIDRRO:
            sites.append((off, 'tpidrro_r', w & 31))
    return sites


def rewrite(image, patch_off, text_end):
    patch = bytearray()
    fixups = []          # (patch_offset, kind) for load-time binding
    counts = {}

    def here():
        return patch_off + len(patch)

    def emit(word, kind=None):
        if kind:
            fixups.append((len(patch), kind))
        patch.extend(struct.pack('<I', word & 0xFFFFFFFF))

    sites = scan_sites(image, text_end)
    for off, kind, reg in sites:
        counts[kind] = counts.get(kind, 0) + 1
        start = here()
        slot = SLOT_BY_KIND[kind]

        if kind == 'tpidr_w':
            emit(STUR(30, 31, -24))
            emit(STUR(0, 31, -16))
            if reg != 1:
                emit(STUR(1, 31, -32))
                emit(MOV_REG(1, reg))
            emit(MOVZ(0, slot))
            emit(BL(here(), 0), 'TLS_SET')
            if reg != 1:
                emit(LDUR(1, 31, -32))
            emit(LDUR(0, 31, -16))
            emit(LDUR(30, 31, -24))
        else:
            emit(STUR(30, 31, -24))
            if reg != 0:
                emit(STUR(0, 31, -16))
            emit(MOVZ(0, slot))
            emit(BL(here(), 0), 'TLS_GET')
            if reg != 0:
                emit(MOV_REG(reg, 0))
                emit(LDUR(0, 31, -16))
            emit(LDUR(30, 31, -24))

        emit(B(here(), off + 4))
        struct.pack_into('<I', image, off, B(off, start))

    return patch, fixups, counts, sites


def main():
    if len(sys.argv) != 3:
        sys.exit('usage: tls_rewrite.py <in.patched.bin> <outdir>')
    src, outdir = sys.argv[1], sys.argv[2]
    image = bytearray(open(src, 'rb').read())

    patch_off = (len(image) + 0xFFFF) & ~0xFFFF
    image.extend(bytes(patch_off - len(image)))
    text_end = 0x4000          # synthetic image text segment

    patch, fixups, counts, sites = rewrite(image, patch_off, text_end)
    image.extend(patch)

    os.makedirs(outdir, exist_ok=True)
    name = 'synthetic'
    open(os.path.join(outdir, f'{name}.android.bin'), 'wb').write(bytes(image))
    with open(os.path.join(outdir, f'{name}.android.fixups.tsv'), 'w') as f:
        f.write(f'# PATCH_OFF\t{patch_off:#x}\n# TEXT_END\t{text_end:#x}\n')
        for p, kind in fixups:
            f.write(f'{patch_off + p:#x}\t{kind}\n')
    with open(os.path.join(outdir, f'{name}.android.sites.tsv'), 'w') as f:
        for off, kind, reg in sites:
            f.write(f'{off:#x}\t{kind}\t{reg}\n')

    print('rewritten', counts, 'sites', len(sites), 'fixups', len(fixups))
    return 0


if __name__ == '__main__':
    sys.exit(main())
