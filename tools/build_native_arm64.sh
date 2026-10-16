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
# Rebuild the object directory from scratch every time: a stale object from an
# earlier configuration (for example an adapter module left over from a build
# that compiled one) would otherwise be linked in silently and collide with the
# real adapter archive.
rm -rf "$OBJ"; mkdir -p "$OBJ"
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

# The runtime never compiles adapter sources.  Direction of the dependency is
# adapter -> runtime: the runtime exposes the host surface (dm_* helpers, g_mod,
# uimsg_emit, ...) and the adapter consumes it.  See d2r-android-adapter's
# docs/CONTRACT.md, whose link test proves exactly this direction.
#
# An adapter build therefore produces its own object files / archive and is
# added at link time by whoever packages them together; the seat it fills while
# it is absent is runtime/adapter_seam.
#
# Point DM_ADAPTER at a d2r-android-adapter checkout that has already run that
# repo's tools/build-android-arm64.sh.  Its archive is linked with --whole-archive
# so every module is pulled in even when nothing references it directly, and its
# strong definitions replace the weak seats above with no other configuration.
ADAPTER=${DM_ADAPTER:-$ROOT/../d2r-adapter-x}
ADAPTER_LIB="$ADAPTER/build/android-arm64/libd2r_adapter.a"
ADAPTER_ARGS=""
if [ -f "$ADAPTER_LIB" ]; then
  echo "adapter: $ADAPTER_LIB"
  ADAPTER_ARGS="-Wl,--whole-archive $ADAPTER_LIB -Wl,--no-whole-archive"
else
  echo "adapter: not built at $ADAPTER_LIB — linking with the inert seats only"
fi

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
           "$HOST"/android_host_contract.c "$HOST"/android_unwind_stub.c \
           "$HOST"/thread.c "$HOST"/crash.c "$HOST"/romfs_validate.c \
           "$HOST"/hle_trace.c "$HOST"/hle_*.c "$HOST"/hle.c "$HOST"/loader.c "$HOST"/android_loader_api.c; do
  name=$(basename "$src" .c)
  # shellcheck disable=SC2086
  clang $CFLAGS -c "$src" -o "$OBJ/$name.o"
done

# The register-preserving TLS trampolines are assembled separately and linked
# into the same shared object as the C bridge.
clang $TGT -fPIC -nostdinc -undef -c "$TLS/tls_trampoline.S" -o "$OBJ/tls_trampoline.o"

clang $TGT -fPIC -nostdinc -undef -c "$HOST/entry.S" -o "$OBJ/host_entry.o"

# Linking lives in its own script so the strict resolution check --no-undefined
# is applied to production builds too, not just to a manual test.
sh "$ROOT/tools/link_native_arm64.sh"

# Package the shared C++ runtime next to the library so the APK is self-contained.
for cxx in "$SYS/usr/lib/aarch64-linux-android/libc++_shared.so" \
           "$SYS/usr/lib/aarch64-linux-android/libc++_shared.so"; do
  [ -f "$cxx" ] && cp -f "$cxx" "$OUT/" && break
done
ls -l "$OUT"
