#!/usr/bin/env python3
"""Build the deterministic synthetic image consumed by loader_test."""
import pathlib, struct, sys

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
size = 0x10000
image = bytearray(size)

# MOD0: loader reads the uint32 at image+4, then the relative dyn offset at
# MOD0+4.  This makes the dynamic table land at 0x200.
struct.pack_into('<I', image, 4, 0x100)
image[0x100:0x104] = b'MOD0'
struct.pack_into('<i', image, 0x104, 0x100)

# AArch64: mov w0, #42; ret.
struct.pack_into('<II', image, 0x1000, 0x52800540, 0xD65F03C0)

# String and symbol tables: null, answer (defined), external (undefined).
strings = b'\0answer\0external\0'
image[0x480:0x480 + len(strings)] = strings
sym = struct.Struct('<IBBHQQ')
sym.pack_into(image, 0x400, 0, 0, 0, 0, 0, 0)
sym.pack_into(image, 0x418, 1, 0x12, 0, 1, 0x1000, 8)
sym.pack_into(image, 0x430, 8, 0x12, 0, 0, 0, 0)

# Dynamic table: RELA, SYMTAB, STRTAB and their sizes.
dyn = struct.Struct('<qQ')
entries = [(6, 0x400), (5, 0x480), (7, 0x600), (8, 3 * 24), (0, 0)]
for i, (tag, value) in enumerate(entries):
    dyn.pack_into(image, 0x200 + i * dyn.size, tag, value)

# RELATIVE -> base + addend; ABS64 -> defined answer + addend;
# GLOB_DAT -> host resolve_import("external").
rela = struct.Struct('<QQq')
rela.pack_into(image, 0x600, 0x5000, 1027, 0x1000)
rela.pack_into(image, 0x618, 0x5008, (1 << 32) | 257, 4)
rela.pack_into(image, 0x630, 0x5010, (2 << 32) | 1025, 0)

(out / 'synthetic.patched.bin').write_bytes(image)
(out / 'synthetic.fixups.tsv').write_text(
    '# PATCH_OFF\t0xc000\n'
    '# KEY_PAGE_OFF\t0x8000\n'
    '# KEY_PAGE_SIZE\t0x4000\n'
    '# TEXT_END\t0x4000\n'
    '# RO_OFF\t0x4000\n'
    '# DATA_OFF\t0x5000\n'
    '# KEY_K18_IDX\t0\n'
    '# KEY_KTLS_IDX\t1\n'
    '# KEY_KTLR_IDX\t2\n'
    '# KEY_KGATE_IDX\t3\n'
    '# KEY_KRES0_IDX\t4\n'
    '# KEY_KRES1_IDX\t5\n'
    '# KEY_KASPECT_IDX\t6\n'
    '# KEY_KASPECTUI_IDX\t7\n'
)
(out / 'synthetic.gates.tsv').write_text('')
print(out / 'synthetic.patched.bin')
