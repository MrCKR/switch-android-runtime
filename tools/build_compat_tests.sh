#!/bin/sh
# Build the compat-layer hash test for Android arm64 and stage it for a device.
set -eu
SDK=${ANDROID_SDK_ROOT:-/var/minis/workspace/android-sdk}
NDK=$(ls -d "$SDK"/ndk/* 2>/dev/null | head -1)
PRE=$NDK/toolchains/llvm/prebuilt/linux-x86_64
SYS=$PRE/sysroot
RT=${DM_RUNTIME:-/var/minis/workspace/switch-android-runtime}
COMPAT=$RT/runtime/compat
OUT=${1:-/tmp/compat-tests}
mkdir -p "$OUT"
mkdir -p /tmp/dm-ssp-stub
: > /tmp/dm-ssp-stub/libssp_nonshared.a
TGT="--target=aarch64-linux-android29 --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"
clang $TGT -O2 -Wall -fno-stack-protector -L/tmp/dm-ssp-stub -I"$COMPAT/darwin" \
  "$RT/tests/test_compat_hashes.c" \
  "$COMPAT/dm_compat_md5.c" "$COMPAT/dm_compat_sha256.c" \
  -o "$OUT/t_hashes"
printf 'staged: %s/t_hashes\n' "$OUT"
if command -v file >/dev/null 2>&1; then file "$OUT/t_hashes" | sed 's/^/  /'; else readelf -h "$OUT/t_hashes" | rg 'Class|Machine' | sed 's/^/  /'; fi
