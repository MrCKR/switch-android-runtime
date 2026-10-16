#!/bin/sh
# Build the real-artifact loader smoke test for Android ARM64.
#
# This test intentionally stops after mapping, parsing, gate registration and
# relocation. It accepts the actual output format made by the user's local NSO
# rewrite, but does not enter the game. The game still needs sdk/RomFS/NVN.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-/var/minis/workspace/android-sdk}
NDK=$(ls -d "$SDK"/ndk/* | head -1)
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
OUT=${1:-/tmp/real-module-load}
UP=${DM_UPSTREAM:-/var/minis/workspace/d2r-upstream}
OBJ="$OUT/obj"
mkdir -p "$OUT" "$OBJ" /tmp/dm-ssp-stub
: > /tmp/dm-ssp-stub/libssp_nonshared.a

python3 "$ROOT/tools/make_android_loader_test_source.py" \
  "$UP/dm/host/loader.c" "$OUT/loader_android.c"

TGT="--target=aarch64-linux-android29 --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"
INC="-I$ROOT/runtime/compat/darwin -I$ROOT/runtime/compat -I$ROOT/runtime/tls \
-I$UP/dm/host -I$UP/dm/gfx/nvn/include"
CF="$TGT -O1 -g -w -fno-stack-protector -fblocks $INC \
-include $ROOT/runtime/compat/darwin/platform_compat.h"

for src in \
  "$OUT/loader_android.c" \
  "$UP/dm/host/hle.c" \
  "$ROOT/runtime/compat/dm_compat_macho.c" \
  "$ROOT/runtime/compat/dm_compat_macho_exec.c" \
  "$ROOT/runtime/compat/dm_compat_sha256.c" \
  "$ROOT/runtime/compat/dm_compat_sha1.c" \
  "$ROOT/tests/real_module_load_test.c"; do
  name=$(basename "$src" .c)
  # shellcheck disable=SC2086
  clang $CF -c "$src" -o "$OBJ/$name.o"
done

clang $TGT -fno-stack-protector -L/tmp/dm-ssp-stub \
  "$OBJ/loader_android.o" "$OBJ/hle.o" "$OBJ/dm_compat_macho.o" \
  "$OBJ/dm_compat_macho_exec.o" "$OBJ/dm_compat_sha256.o" \
  "$OBJ/dm_compat_sha1.o" "$OBJ/real_module_load_test.o" \
  -ldl -o "$OUT/real_module_load_test"

readelf -h "$OUT/real_module_load_test" | rg 'Class|Machine' | sed 's/^/  /'
printf '%s\n' "$OUT/real_module_load_test"
