#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-/var/minis/workspace/android-sdk}
NDK=$(ls -d "$SDK"/ndk/* 2>/dev/null | head -1)
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"; SYS="$PRE/sysroot"
OUT=${1:-/tmp/tls-test}; mkdir -p "$OUT" /tmp/dm-ssp-stub
: > /tmp/dm-ssp-stub/libssp_nonshared.a
TGT="--target=aarch64-linux-android29 --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"
clang $TGT -O2 -Wall -fno-stack-protector -I"$ROOT/runtime/tls" \
  "$ROOT/runtime/tls/android_tls.c" "$ROOT/tests/tls_test.c" \
  -L/tmp/dm-ssp-stub -o "$OUT/tls_test"
readelf -h "$OUT/tls_test" | rg 'Class|Machine' | sed 's/^/  /'
printf 'tls test binary: %s/tls_test\n' "$OUT"
