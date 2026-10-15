#!/bin/sh
# Build the guest address-space probe as a native Android ARM64 executable.
#
# Same toolchain route as build_loader_test.sh: on an ARM64 build host the
# NDK's host clang is already the target architecture, so --target plus
# --sysroot produces correct aarch64 ABI code with no cross-architecture
# emulation of the probe itself.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/var/minis/workspace/android-sdk}}
OUT=${1:-/tmp/dm-as-probe}
API=${DM_ANDROID_API:-29}

NDK=$(ls -d "$SDK"/ndk/* 2>/dev/null | head -1)
[ -n "$NDK" ] || { echo "no NDK under $SDK" >&2; exit 1; }
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
CC=${DM_CC:-clang}

mkdir -p "$OUT"
# The host toolchain on an ARM64 build machine has no libssp_nonshared; the
# Android target does not need it. An empty archive satisfies the driver.
mkdir -p /tmp/dm-ssp-stub
: > /tmp/dm-ssp-stub/libssp_nonshared.a

TGT="--target=aarch64-linux-android$API --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"

$CC $TGT -O1 -g -Wall -Wextra -static -fno-stack-protector \
    -L/tmp/dm-ssp-stub -o "$OUT/guest_as_probe" "$ROOT/tests/guest_as_probe.c"

echo "built $OUT/guest_as_probe"
file "$OUT/guest_as_probe" 2>/dev/null || true
