#!/bin/sh
# Build a deterministic synthetic module and the upstream loader against it.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-/var/minis/workspace/android-sdk}
NDK=$(ls -d "$SDK"/ndk/* 2>/dev/null | head -1)
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
UP=${DM_UPSTREAM:-/var/minis/workspace/d2r-upstream}
OUT=${1:-/tmp/loader-test}
OBJ="$OUT/obj"
mkdir -p "$OBJ"
python3 "$ROOT/tools/gen_synthetic_module.py" "$OUT"
python3 "$ROOT/tools/make_android_loader_test_source.py" "$UP/dm/host/loader.c" "$OUT/loader_android.c"
: > /tmp/dm-ssp-stub/libssp_nonshared.a
TGT="--target=aarch64-linux-android29 --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"
CFLAGS="$TGT -O1 -g -fno-stack-protector -fblocks -I$ROOT/runtime/compat/darwin -I$ROOT/runtime/compat -I$UP/dm/host -I$UP/dm/gfx/nvn/include -include $ROOT/runtime/compat/darwin/platform_compat.h"

for src in \
  "$OUT/loader_android.c" \
  "$ROOT/runtime/compat/dm_compat_macho.c" \
  "$ROOT/runtime/compat/dm_compat_macho_exec.c" \
  "$ROOT/runtime/compat/dm_compat_sha256.c" \
  "$ROOT/tests/loader_test_main.c"; do
  name=$(basename "$src" .c)
  # shellcheck disable=SC2086
  clang $CFLAGS -c "$src" -o "$OBJ/$name.o"
done

clang $TGT -fno-stack-protector -L/tmp/dm-ssp-stub \
  "$OBJ/loader_android.o" "$OBJ/dm_compat_macho.o" "$OBJ/dm_compat_macho_exec.o" \
  "$OBJ/dm_compat_sha256.o" "$OBJ/loader_test_main.o" \
  -o "$OUT/loader_test"
readelf -h "$OUT/loader_test" | rg 'Class|Machine' | sed 's/^/  /'
printf 'synthetic loader test: %s/loader_test\n' "$OUT"
