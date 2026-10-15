#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/var/minis/workspace/android-sdk}}
NDK="$SDK/ndk/27.0.12077973"
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
OUT="$ROOT/app/src/main/jniLibs/arm64-v8a"
OBJ="$ROOT/build/compat-objects"
mkdir -p "$OUT" "$OBJ" "$ROOT/build/host-libs"
: > "$ROOT/build/host-libs/libssp_nonshared.a"

TGT="--target=aarch64-linux-android29 --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"
COMPAT="$ROOT/runtime/compat"
TLS="$ROOT/runtime/tls"
CFLAGS="$TGT -fPIC -std=c11 -fno-stack-protector -fblocks -I$COMPAT/darwin -I$COMPAT -I$TLS -include $COMPAT/darwin/platform_compat.h"

# Compile the C compatibility layer as C, not as C++, so the upstream-facing
# ABI remains exactly the one validated by compile_upstream_modules.sh.
for src in "$COMPAT"/dm_compat_*.c "$TLS"/android_tls.c "$TLS"/android_tls_thunk.c; do
  name=$(basename "$src" .c)
  # shellcheck disable=SC2086
  clang $CFLAGS -c "$src" -o "$OBJ/$name.o"
done
# The register-preserving TLS trampolines are assembled separately and linked
# into the same shared object as the C bridge.
clang $TGT -fPIC -nostdinc -undef -c "$TLS/tls_trampoline.S" -o "$OBJ/tls_trampoline.o"

clang++ $TGT -fPIC -shared -std=c++20 -fno-stack-protector \
  -nostdinc++ -isystem "$SYS/usr/include/c++/v1" \
  "$ROOT/app/src/main/cpp/runtime.cpp" "$OBJ"/*.o \
  -L"$ROOT/build/host-libs" \
  -L"$SYS/usr/lib/aarch64-linux-android/29" \
  -L"$SYS/usr/lib/aarch64-linux-android" \
  -L"$PRE/lib/clang/18/lib/linux" \
  -rtlib=compiler-rt -unwindlib=none \
  -lc++_shared -landroid -llog -laaudio -lvulkan \
  -o "$OUT/libswitch_runtime.so"

# Package the shared C++ runtime next to the library so the APK is self-contained.
for cxx in "$SYS/usr/lib/aarch64-linux-android/libc++_shared.so" \
           "$SYS/usr/lib/aarch64-linux-android/libc++_shared.so"; do
  [ -f "$cxx" ] && cp -f "$cxx" "$OUT/" && break
done
ls -l "$OUT"
