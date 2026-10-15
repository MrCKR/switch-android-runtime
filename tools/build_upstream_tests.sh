#!/bin/sh
# Build upstream dm/gfx unit tests as native Android ARM64 executables.
#
# The tests exercise logic that is platform independent apart from a narrow
# Darwin API surface (mach clock, QoS, task/thread introspection). Those calls
# are satisfied by the shims in runtime/compat/darwin, which are placed FIRST on
# the include path so "mach/..." resolves there instead of failing.
#
# Order for a modern NDK: the host clang that ships with the NDK is x86_64 while
# the NDK's sysroot holds the real Android libraries. On an ARM64 build host the
# host compiler is aarch64, so we drive it with --target and point --sysroot at
# the NDK, which yields correct aarch64 ABI code without cross-architecture
# emulation of the target.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/var/minis/workspace/android-sdk}}
UPSTREAM=${DM_UPSTREAM:-/var/minis/workspace/d2r-upstream}
OUT=${1:-/tmp/dm-tests-android}
API=${DM_ANDROID_API:-29}

NDK=$(ls -d "$SDK"/ndk/* 2>/dev/null | head -1)
[ -n "$NDK" ] || { echo "no NDK under $SDK" >&2; exit 1; }
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
CC=${DM_CC:-clang}

[ -d "$UPSTREAM/dm/gfx/tests" ] || { echo "upstream tests not found: $UPSTREAM" >&2; exit 1; }

# The host toolchain on an ARM64 build machine has no libssp_nonshared; the
# Android target does not need it. An empty archive satisfies the driver.
mkdir -p /tmp/dm-ssp-stub
: > /tmp/dm-ssp-stub/libssp_nonshared.a

COMPAT="$ROOT/runtime/compat/darwin"
mkdir -p "$OUT"
cd "$UPSTREAM/dm/gfx/tests"

CF="-O1 -g -Wall -Wno-unused-function -I../nvn/include -I$COMPAT"
TGT="--target=aarch64-linux-android$API --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"
LD="-fno-stack-protector -L/tmp/dm-ssp-stub"

# Pure C tests plus the ones that only need the Darwin shim. Metal/MetalKit
# tests are excluded: they require a translated graphics backend, not a shim.
TESTS="t_hmap t_pace t_cover"
for t in $TESTS; do
    # shellcheck disable=SC2086
    $CC $TGT $CF $LD -o "$OUT/$t" "$t.c"
    echo "built $t"
done

echo "output: $OUT"
