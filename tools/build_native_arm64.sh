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

# Note: do NOT add -resource-dir here. The build uses the host clang, and
# pointing -resource-dir at the NDK's clang 18 directory mixes a clang 19
# frontend with clang 18's arm_neon.h, which breaks any NEON-using source
# (libdeflate's adler32 was the first to hit it).
TGT="--target=aarch64-linux-android29 --sysroot=$SYS"
COMPAT="$ROOT/runtime/compat"
TLS="$ROOT/runtime/tls"
HOST="$ROOT/runtime/host"
CFLAGS="$TGT -fPIC -std=gnu11 -fno-stack-protector -fblocks -I$COMPAT/darwin -I$COMPAT -I$TLS -I$HOST -I$ROOT/runtime/third_party/libdeflate -I$ROOT/runtime/third_party/libdeflate/lib -include $COMPAT/darwin/platform_compat.h"

# Compile the C compatibility layer as C, not as C++, so the upstream-facing
# ABI remains exactly the one validated by compile_upstream_modules.sh.
for src in "$COMPAT"/dm_compat_*.c "$TLS"/android_tls.c "$TLS"/android_tls_thunk.c \
           "$ROOT"/runtime/third_party/libdeflate/lib/adler32.c \
           "$ROOT"/runtime/third_party/libdeflate/lib/arm/cpu_features.c \
           "$ROOT"/runtime/third_party/libdeflate/lib/deflate_decompress.c \
           "$ROOT"/runtime/third_party/libdeflate/lib/utils.c \
           "$ROOT"/runtime/third_party/libdeflate/lib/zlib_decompress.c \
           "$ROOT"/runtime/adapter_seam/*.c \
           "$HOST"/android_host_defaults.c "$HOST"/android_boot_stubs.c \
           "$HOST"/thread.c "$HOST"/crash.c "$HOST"/romfs_validate.c \
           "$HOST"/hle_*.c "$HOST"/hle.c "$HOST"/loader.c "$HOST"/android_loader_api.c; do
  name=$(basename "$src" .c)
  # shellcheck disable=SC2086
  clang $CFLAGS -c "$src" -o "$OBJ/$name.o"
done

# The register-preserving TLS trampolines are assembled separately and linked
# into the same shared object as the C bridge.
clang $TGT -fPIC -nostdinc -undef -c "$TLS/tls_trampoline.S" -o "$OBJ/tls_trampoline.o"

clang $TGT -fPIC -nostdinc -undef -c "$HOST/entry.S" -o "$OBJ/host_entry.o"

clang++ $TGT -resource-dir "$PRE/lib/clang/18" -fPIC -shared -std=c++20 -fno-stack-protector \
  -nostdinc++ -isystem "$SYS/usr/include/c++/v1" -I"$HOST" \
  "$ROOT/app/src/main/cpp/runtime.cpp" "$OBJ"/*.o \
  -L"$ROOT/build/host-libs" \
  -L"$SYS/usr/lib/aarch64-linux-android/29" \
  -L"$SYS/usr/lib/aarch64-linux-android" \
  -L"$PRE/lib/clang/18/lib/linux" \
  -L/tmp/rt-stub \
  -rtlib=compiler-rt -unwindlib=none \
  -lc++_shared -landroid -llog -laaudio -lvulkan \
  -o "$OUT/libswitch_runtime.so"

# Package the shared C++ runtime next to the library so the APK is self-contained.
for cxx in "$SYS/usr/lib/aarch64-linux-android/libc++_shared.so" \
           "$SYS/usr/lib/aarch64-linux-android/libc++_shared.so"; do
  [ -f "$cxx" ] && cp -f "$cxx" "$OUT/" && break
done
ls -l "$OUT"
