#!/bin/sh
# Build the TLS rewrite end-to-end test:
#   1. assemble the synthetic guest routine and place its code at text offset
#      0x1000 of a synthetic image (the same place gen_synthetic_module.py puts
#      the entry point);
#   2. run tls_rewrite.py over that image;
#   3. compile the binder/assert harness against the real trampolines.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-/var/minis/workspace/android-sdk}
NDK=$(ls -d "$SDK"/ndk/* 2>/dev/null | head -1)
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
OUT=${1:-/tmp/tls-rewrite-test}
TLS="$ROOT/runtime/tls"
OBJ="$OUT/obj"
mkdir -p "$OBJ" /tmp/dm-ssp-stub
: > /tmp/dm-ssp-stub/libssp_nonshared.a

TGT="--target=aarch64-linux-android29 --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"

# 1. Assemble the guest routine and extract its code bytes.
clang $TGT -fno-stack-protector -c "$ROOT/tests/tls_guest_routine.S" -o "$OBJ/guest.o"
llvm-objcopy -O binary --only-section=.text "$OBJ/guest.o" "$OUT/guest.text"
python3 - "$OUT/guest.text" "$OUT/image.bin" <<'PY'
import sys, pathlib
code = pathlib.Path(sys.argv[1]).read_bytes()
size = 0x10000
img = bytearray(size)
img[0x1000:0x1000 + len(code)] = code
pathlib.Path(sys.argv[2]).write_bytes(img)
print(f'placed {len(code)} bytes of guest code at 0x1000')
PY

# 2. Rewrite it.
python3 "$ROOT/tools/tls_rewrite.py" "$OUT/image.bin" "$OUT"

# 3. Build the harness with the trampolines and the bridge.
clang $TGT -O1 -fno-stack-protector -I"$TLS" \
  "$TLS/android_tls.c" "$TLS/android_tls_thunk.c" "$TLS/tls_trampoline.S" \
  "$ROOT/tests/tls_rewrite_test.c" \
  -L/tmp/dm-ssp-stub -o "$OUT/tls_rewrite_test"

readelf -h "$OUT/tls_rewrite_test" | rg 'Class|Machine' | sed 's/^/  /'
printf 'tls rewrite test: %s/tls_rewrite_test\n' "$OUT"
