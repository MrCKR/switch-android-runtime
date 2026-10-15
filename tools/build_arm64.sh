#!/bin/sh
# End-to-end build on an ARM64 host: native stage + Gradle packaging.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SDK=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/var/minis/workspace/android-sdk}}
export ANDROID_HOME="$SDK" ANDROID_SDK_ROOT="$SDK"

"$ROOT/tools/build_native_arm64.sh"

cd "$ROOT"
gradle --no-daemon assembleDebug

APK="$ROOT/app/build/outputs/apk/debug/app-debug.apk"
[ -f "$APK" ] || { echo "APK not produced" >&2; exit 1; }
printf '%s\n' "APK: $APK"
