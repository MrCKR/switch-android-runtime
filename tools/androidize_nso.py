#!/usr/bin/env python3
"""Convert patch_nso.py's Apple sysreg stubs to Android TLS thunks.

patch_nso.py intentionally emits the Apple inline tpidrro key-page sequence.
On Android TPIDRRO_EL0 is zero/read-only, so every sysreg read/write in the
already-generated patch area is redirected through the preserving thunk.
"""
import os, struct, sys

MRS_TPIDR = 0xD53BD040
MSR_TPIDR = 0xD51BD040
MRS_TPIDRRO = 0xD53BD060

def B(frm, to):
    off = to - frm
    if off % 4 or not (-(1 << 27) <= off < (1 << 27)):
        raise SystemExit(f'branch out of range: {frm:#x}->{to:#x}')
    return 0x14000000 | ((off >> 2) & 0x3ffffff)

def BL(frm):
    return 0x94000000

def MOVZ(reg, imm):
    return 0xD2800000 | (imm << 5) | reg

def MOV_REG(dst, src):
    return 0xAA0003E0 | (src << 16) | dst

def STUR(rt, rn, off):
    return 0xF8000000 | ((off & 0x1ff) << 12) | (rn << 5) | rt

def LDUR(rt, rn, off):
    return 0xF8400000 | ((off & 0x1ff) << 12) | (rn << 5) | rt

def u32(d, o): return struct.unpack_from('<I', d, o)[0]
def put(d, o, v): struct.pack_into('<I', d, o, v & 0xffffffff)

def main():
    if len(sys.argv) != 3:
        raise SystemExit('usage: androidize_nso.py <outdir> <module>')
    outdir, name = sys.argv[1:]
    image = bytearray(open(os.path.join(outdir, name + '.patched.bin'), 'rb').read())
    fixpath = os.path.join(outdir, name + '.fixups.tsv')
    patch_off = text_end = None
    for line in open(fixpath):
        if line.startswith('# PATCH_OFF'):
            patch_off = int(line.split()[-1], 0)
        elif line.startswith('# TEXT_END'):
            text_end = int(line.split()[-1], 0)
    if patch_off is None or text_end is None:
        raise SystemExit('missing PATCH_OFF/TEXT_END')

    original_end = len(image)
    patch = bytearray()
    gets, sets = [], []
    sites = []

    def here(): return original_end + len(patch)
    def emit(w): patch.extend(struct.pack('<I', w & 0xffffffff))

    # Scan the appended Apple patch blocks emitted by patch_nso.py.  The
    # original text branches into these blocks; rewriting the sysreg inside
    # each block is the Android conversion step.
    for off in range(patch_off, original_end, 4):
        w = u32(image, off)
        if (w & 0xffffffe0) == MRS_TPIDR or (w & 0xffffffe0) == MRS_TPIDRRO:
            kind = 'GET'; reg = w & 31; slot = 1 if (w & 0xffffffe0) == MRS_TPIDR else 2
            start = here()
            tmp = 16 if reg != 16 else 17
            if reg == 30:
                emit(STUR(tmp, 31, -24))
            else:
                emit(STUR(30, 31, -24)); emit(STUR(tmp, 31, -32))
            if reg != 0: emit(STUR(0, 31, -16))
            emit(MOVZ(0, slot))
            call = here(); emit(BL(call)); gets.append(call)
            if reg != 0: emit(MOV_REG(reg, 0))
            if reg != 0: emit(LDUR(0, 31, -16))
            if reg == 30:
                emit(LDUR(tmp, 31, -24))
            else:
                emit(LDUR(tmp, 31, -32)); emit(LDUR(30, 31, -24))
            emit(B(here(), off + 4))
            put(image, off, B(off, start)); sites.append((off, kind, reg, slot))
        elif (w & 0xffffffe0) == MSR_TPIDR:
            reg = w & 31; slot = 1; start = here()
            emit(STUR(30, 31, -24)); emit(STUR(0, 31, -16))
            if reg != 1:
                emit(STUR(1, 31, -32)); emit(MOV_REG(1, reg))
            emit(MOVZ(0, slot)); call = here(); emit(BL(call)); sets.append(call)
            if reg != 1: emit(LDUR(1, 31, -32))
            emit(LDUR(0, 31, -16)); emit(LDUR(30, 31, -24)); emit(B(here(), off + 4))
            put(image, off, B(off, start)); sites.append((off, 'SET', reg, slot))

    image.extend(patch)
    with open(os.path.join(outdir, name + '.patched.bin'), 'wb') as f: f.write(image)
    with open(fixpath, 'a') as f:
        for p in gets: f.write(f'# ANDROID_TLS_GET {p:#x}\n')
        for p in sets: f.write(f'# ANDROID_TLS_SET {p:#x}\n')
    with open(os.path.join(outdir, name + '.android-tls.tsv'), 'w') as f:
        for row in sites: f.write(f'{row[0]:#x}\t{row[1]}\tx{row[2]}\tK{row[3]}\n')
    print(name, 'android_tls', {'get': len(gets), 'set': len(sets)}, 'sites', len(sites), 'new_bytes', len(patch))

if __name__ == '__main__': main()
