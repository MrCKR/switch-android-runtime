#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$ROOT/../../android-sdk}}
NDK="$SDK/ndk/27.0.12077973"
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
OUT="$ROOT/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$OUT" "$ROOT/build/host-libs"
: > "$ROOT/build/host-libs/libssp_nonshared.a"
clang++ \
  --target=aarch64-linux-android29 \
  --sysroot="$SYS" \
  -resource-dir="$PRE/lib/clang/18" \
  -fPIC -shared -std=c++20 -fno-stack-protector \
  -nostdinc++ -isystem "$SYS/usr/include/c++/v1" \
  "$ROOT/app/src/main/cpp/runtime.cpp" \
  -L"$ROOT/build/host-libs" \
  -L"$SYS/usr/lib/aarch64-linux-android/29" \
  -L"$SYS/usr/lib/aarch64-linux-android" \
  -L"$PRE/lib/clang/18/lib/linux" \
  -rtlib=compiler-rt -unwindlib=none \
  -lc++_shared -landroid -llog -lvulkan \
  -o "$OUT/libswitch_runtime.so"
