#!/usr/bin/env python3
"""Android conversion of patch_nso.py's guest slot access.

patch_nso.py emits the Apple sequence for every guest TLS / thread-slot access:

    mrs  xB, tpidrro_el0        ; thread base            (xB = base register)
    adrp xK, key_page
    ldr  xK, [xK, #8*key]       ; key-page entry          (host thread-key value * 8)
    add  xB, xB, xK             ; slot address            (base + offset)
    ldr/str xV, [xB]            ; the actual access

On Android that cannot work: TPIDRRO_EL0 reads as 0 and writing it is SIGILL,
and the key page holds host pthread_key_t values (hundreds of millions, so
value*8 is an out-of-range address).

Rather than rewriting the surrounding sequence (which differs per site shape),
this pass replaces ONLY the mrs with a call through a veneer to
dm_guest_tls_base(), and makes each key-page entry hold the offset of the
matching slot inside the guest control block that the base pointer refers to.
The generic part of every sequence then stays byte-identical to the version
already validated on Apple, and only the two Android-specific values change.

The veneer is emitted into the appended patch area, so the call itself can be
resolved offline; only the veneer's literal (the host function address) needs
load-time binding.
"""
import os, struct, sys

MRS_TPIDRRO = 0xD53BD060
SLOT_STRIDE = 0x4000
# Slot order must match the KEY_*_IDX order in patch_nso.py.
SLOTS = ['K18', 'KTLS', 'KTLR', 'KGATE', 'KRES0', 'KRES1', 'KASPECT', 'KASPECTUI']

def B(frm, to):
    off = to - frm
    if off % 4 or not (-(1 << 27) <= off < (1 << 27)):
        raise SystemExit(f'branch out of range: {frm:#x}->{to:#x}')
    return 0x14000000 | ((off >> 2) & 0x3ffffff)

def BL(frm, to):
    off = to - frm
    if off % 4 or not (-(1 << 27) <= off < (1 << 27)):
        raise SystemExit(f'bl out of range: {frm:#x}->{to:#x}')
    return 0x94000000 | ((off >> 2) & 0x3ffffff)

def MOVZ(reg, imm):
    return 0xD2800000 | ((imm & 0xffff) << 5) | reg

def STR(rt, rn, off):
    assert off % 8 == 0 and 0 <= off < 32768
    return 0xF9000000 | ((off // 8) << 10) | (rn << 5) | rt

def LDR(rt, rn, off):
    assert off % 8 == 0 and 0 <= off < 32768
    return 0xF9400000 | ((off // 8) << 10) | (rn << 5) | rt

def MOV_REG(dst, src):
    return 0xAA0003E0 | (src << 16) | dst

def ADDI(reg, src, imm):
    return 0x91000000 | ((imm & 0xfff) << 10) | (src << 5) | reg

def SUBI(reg, src, imm):
    return 0xD1000000 | ((imm & 0xfff) << 10) | (src << 5) | reg

def u32(d, o): return struct.unpack_from('<I', d, o)[0]
def put(d, o, v): struct.pack_into('<I', d, o, v & 0xffffffff)

FRAME = 32   # 16-byte aligned: x30 at +0, scratch at +8

def main():
    if len(sys.argv) != 3:
        raise SystemExit('usage: androidize_nso.py <outdir> <module>')
    outdir, name = sys.argv[1:]
    image = bytearray(open(os.path.join(outdir, name + '.patched.bin'), 'rb').read())
    fixpath = os.path.join(outdir, name + '.fixups.tsv')
    header = {}
    for line in open(fixpath):
        if line.startswith('#'):
            p = line.split()
            if len(p) >= 3:
                try:
                    header[p[1]] = int(p[2], 0)
                except ValueError:
                    pass
    patch_off = header.get('PATCH_OFF')
    key_page_off = header.get('KEY_PAGE_OFF')
    key_page_size = header.get('KEY_PAGE_SIZE')
    text_end = header.get('TEXT_END')
    if not patch_off or not key_page_off or not key_page_size:
        raise SystemExit('fixups header missing PATCH_OFF/KEY_PAGE_OFF/KEY_PAGE_SIZE')

    # 1) Key page: host pthread_key_t values -> slot offsets in the guest block.
    page = key_page_off
    for i, slot in enumerate(SLOTS):
        key_idx = header.get('KEY_' + slot + '_IDX')
        if key_idx is None:
            continue
        put(image, page + key_idx * 8, i * SLOT_STRIDE)

    mouse = SLOTS.index('KASPECTUI') * SLOT_STRIDE  # keep a reference for the check below
    original_end = len(image)
    patch = bytearray()

    def here(): return original_end + len(patch)
    def emit(w): patch.extend(struct.pack('<I', w & 0xffffffff))

    # 2) Veneer: ldr x16, #8 ; br x16 ; .quad <host fn, bound at load time>
    veneer = here()
    emit(0x58000050)          # ldr x16, #8
    emit(0xD61F0200)          # br x16
    literal = here()
    emit(0)                   # patched by the loader
    emit(0)                   # padding

    calls = []
    sites = []
    # patch_nso.py already moved every original sysreg out of the text
    # segment into the appended patch area, so scan from there to the end of
    # the Apple image.
    for off in range(patch_off, original_end, 4):
        w = u32(image, off)
        if (w & 0xffffffe0) != MRS_TPIDRRO:
            continue
        reg = w & 31
        start = here()
        # The trampoline preserves x1-x18 and x30 but NOT x0 (that is where it
        # returns the base).  x0 must survive around this call for two reasons:
        # gate blocks pass the svc arguments in x0..x2, and the lock sequences
        # use x0 as a live operand.  So stash x0 and restore it afterwards --
        # unless x0 IS the target register, which only happens for a real
        # slot read.  x30 likewise must not be restored when it is the target.
        emit(SUBI(31, 31, FRAME))
        emit(STR(30, 31, 0))
        emit(STR(0, 31, 8))
        emit(BL(here(), veneer))
        if reg != 0:
            emit(MOV_REG(reg, 0))
            emit(LDR(0, 31, 8))
        if reg != 30:
            emit(LDR(30, 31, 0))
        emit(ADDI(31, 31, FRAME))
        emit(B(here(), off + 4))
        put(image, off, B(off, start))
        sites.append((off, reg))

    image.extend(patch)
    with open(os.path.join(outdir, name + '.patched.bin'), 'wb') as f:
        f.write(image)
    with open(fixpath, 'a') as f:
        f.write(f'# ANDROID_TLS_BASE_VENEER\t{literal:#x}\n')
    with open(os.path.join(outdir, name + '.android-tls.tsv'), 'w') as f:
        for off, reg in sites:
            f.write(f'{off:#x}\tmrs\tx{reg}\n')
    print(name, 'android_tls_base', len(sites), 'sites', 'veneer', hex(literal),
          'new_bytes', len(patch))

if __name__ == '__main__': main()
