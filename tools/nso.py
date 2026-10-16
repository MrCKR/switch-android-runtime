#!/usr/bin/env python3
"""nso.py — read and write Switch NSO images.

The rewriter (patch_nso.py) needs lz4.block, which is not installable here for
this Python. The shared library is present, so this module binds it through
ctypes and exposes the same two calls the rewriter uses.

Also provides the container handling the rewriter does inline, so the runtime's
own loader and the probe tools can share one implementation.
"""
import ctypes
import ctypes.util
import struct


# ---- lz4 block format, via the system library ---------------------------

class _LZ4:
    def __init__(self):
        name = ctypes.util.find_library("lz4") or "liblz4.so.1"
        self.lib = ctypes.CDLL(name)
        self.lib.LZ4_compressBound.argtypes = [ctypes.c_int]
        self.lib.LZ4_compressBound.restype = ctypes.c_int
        self.lib.LZ4_compress_default.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
        self.lib.LZ4_compress_default.restype = ctypes.c_int
        self.lib.LZ4_decompress_safe.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
        self.lib.LZ4_decompress_safe.restype = ctypes.c_int

    def compress(self, src: bytes) -> bytes:
        bound = self.lib.LZ4_compressBound(len(src))
        dst = ctypes.create_string_buffer(bound)
        n = self.lib.LZ4_compress_default(src, dst, len(src), bound)
        if n <= 0:
            raise RuntimeError("LZ4_compress_default failed")
        return dst.raw[:n]

    def decompress(self, src: bytes, uncompressed_size: int) -> bytes:
        dst = ctypes.create_string_buffer(uncompressed_size)
        n = self.lib.LZ4_decompress_safe(src, dst, len(src), uncompressed_size)
        if n < 0:
            raise RuntimeError("LZ4_decompress_safe failed")
        if n != uncompressed_size:
            raise RuntimeError(
                f"decompressed {n} bytes, header promised {uncompressed_size}")
        return dst.raw[:n]


_lz4 = _LZ4()


class _Block:
    compress = staticmethod(lambda data: _lz4.compress(data))
    decompress = staticmethod(
        lambda data, uncompressed_size: _lz4.decompress(data, uncompressed_size))


class _Mod:
    block = _Block


# Drop-in for `import lz4.block`, for the upstream rewriter.
lz4 = _Mod


# ---- NSO container ------------------------------------------------------

NSO_MAGIC = b"NSO0"
# Header layout: magic, version, flags, then three (file_off, mem_off, size)
# triples, then bss size, then the build ID (0x40..0x50), then the three
# compressed sizes at 0x60.
FLAGS_OFF = 0x0C
SEGS_OFF = 0x10
BSS_OFF = 0x3C
BUILD_ID_OFF = 0x40
BUILD_ID_LEN = 0x10
CSIZE_OFF = 0x60
HEADER_SIZE = 0x100

SEG_NAMES = ("text", "rodata", "data")


class Nso:
    def __init__(self, path):
        self.path = path
        raw = open(path, "rb").read()
        if raw[:4] != NSO_MAGIC:
            raise ValueError(f"{path}: not an NSO (magic {raw[:4]!r})")
        self.raw = raw
        self.flags = struct.unpack_from("<I", raw, FLAGS_OFF)[0]
        self.segs = [struct.unpack_from("<III", raw, SEGS_OFF + i * 0x10)
                     for i in range(3)]
        self.bss = struct.unpack_from("<I", raw, BSS_OFF)[0]
        self.build_id = raw[BUILD_ID_OFF:BUILD_ID_OFF + BUILD_ID_LEN].hex()
        self.csize = struct.unpack_from("<III", raw, CSIZE_OFF)

        # Decompress into one flat image, indexed by memory offset.
        total = self.segs[2][1] + self.segs[2][2] + self.bss
        self.image = bytearray(total)
        self.seg_offsets = {}
        for i, (fo, mo, sz) in enumerate(self.segs):
            if fo == 0 and sz == 0:
                continue
            comp = raw[fo:fo + self.csize[i]]
            data = (_lz4.decompress(comp, sz) if self.flags >> i & 1 else comp)
            if len(data) != sz:
                raise RuntimeError(
                    f"segment {SEG_NAMES[i]}: got {len(data)} bytes, expected {sz}")
            self.image[mo:mo + sz] = data
            self.seg_offsets[SEG_NAMES[i]] = (mo, sz)

    @property
    def text_end(self):
        return self.segs[0][2]

    def summary(self):
        lines = [f"{self.path}",
                 f"  build id {self.build_id}",
                 f"  flags    {self.flags:#x} "
                 f"compressed={[bool(self.flags >> i & 1) for i in range(3)]}"]
        for i, (fo, mo, sz) in enumerate(self.segs):
            lines.append(f"  {SEG_NAMES[i]:6s} file={fo:#010x} mem={mo:#010x} "
                         f"size={sz:#010x} ({sz / 1048576:.1f} MB) "
                         f"csize={self.csize[i]:#x}")
        lines.append(f"  bss    {self.bss:#x} ({self.bss / 1048576:.1f} MB)")
        lines.append(f"  image  {len(self.image):#x} ({len(self.image) / 1048576:.1f} MB)")
        return "\n".join(lines)


if __name__ == "__main__":
    import sys
    for p in sys.argv[1:]:
        print(Nso(p).summary())
