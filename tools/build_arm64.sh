#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$ROOT/../../android-sdk}}
BT="$SDK/build-tools/35.0.0"
OUT="$ROOT/build/apk"
mkdir -p "$OUT"
"$ROOT/tools/build_native_arm64.sh"
# Gradle remains the canonical resource/manifest/JNI packager on hosts with a native Android build toolchain.
# This script exists as the deterministic ARM64 native stage and fails early if SDK/NDK paths are absent.
if [ ! -x "$BT/aapt2" ] && [ ! -x "$BT/aapt2.x86_64" ]; then
  echo "Android build-tools missing: $BT" >&2; exit 2
fi
printf '%s\n' "Native ARM64 library: $ROOT/app/src/main/jniLibs/arm64-v8a/libswitch_runtime.so"
