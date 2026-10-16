#!/bin/sh
# Link the runtime, optionally with the adapter archive, and FAIL on any symbol
# that will not resolve at run time.
#
# Why this exists: linking with plain -shared let a library through with 22
# unresolved symbols, and the mistake only showed up on the device as
#
#     UnsatisfiedLinkError: cannot locate symbol "g_dm_frz_want"
#
# because a shared object is permitted to carry undefined symbols but Android's
# dlopen is strict.  -Wl,--no-undefined turns that device-side failure into a
# build error.  Platform libraries (Vulkan, AAudio, libc, libc++) are exempted
# through --allow-shlib-undefined because they are supplied by the system.
#
#   DM_CHECK_ONLY=1   link to a temp file instead of the APK jniLibs directory
set -eu
R=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/var/minis/workspace/android-sdk}}
NDK="$SDK/ndk/27.0.12077973"
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
OBJ="$R/build/compat-objects"
ADAPTER=${DM_ADAPTER:-$R/../d2r-adapter-x}
ADAPTER_LIB="$ADAPTER/build/android-arm64/libd2r_adapter.a"

if [ "${DM_CHECK_ONLY:-0}" = "1" ]; then
  OUT=/tmp/linkcheck.so
else
  OUT="$R/app/src/main/jniLibs/arm64-v8a/libswitch_runtime.so"
fi

ADAPTER_ARGS=""
if [ -f "$ADAPTER_LIB" ]; then
  echo "adapter: $ADAPTER_LIB"
  # --whole-archive: pull in every module even when nothing references it
  # directly, so the archive's symbols are checked like the rest.
  ADAPTER_ARGS="-Wl,--whole-archive $ADAPTER_LIB -Wl,--no-whole-archive"
else
  echo "adapter: not built at $ADAPTER_LIB — checking the runtime alone"
fi

TGT="--target=aarch64-linux-android29 --sysroot=$SYS"

# shellcheck disable=SC2086
clang++ $TGT -resource-dir "$PRE/lib/clang/18" -fPIC -shared -std=c++20 -fno-stack-protector \
  -nostdinc++ -isystem "$SYS/usr/include/c++/v1" -I"$R/runtime/host" \
  "$R/app/src/main/cpp/runtime.cpp" "$OBJ"/*.o \
  $ADAPTER_ARGS \
  -Wl,--no-undefined -Wl,--allow-shlib-undefined \
  -L"$R/build/host-libs" \
  -L"$SYS/usr/lib/aarch64-linux-android/29" \
  -L"$SYS/usr/lib/aarch64-linux-android" \
  -L"$PRE/lib/clang/18/lib/linux" \
  -L/tmp/rt-stub \
  -rtlib=compiler-rt -unwindlib=none \
  -lc++_shared -landroid -llog -laaudio -lvulkan \
  -o "$OUT"

echo "linked ok: $OUT"
if [ "${DM_CHECK_ONLY:-0}" != "1" ]; then ls -l "$OUT"; fi
