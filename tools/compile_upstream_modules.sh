#!/bin/sh
# Compile upstream dm/host modules for Android arm64 with the Darwin compat
# layer, and report the pass/fail matrix.
#
# The compat layer's POSIX shims are headers, but upstream does not include all
# of them (hle_time.c includes only hle.h/time.h). Rather than edit upstream
# source, force-include the shim header so every translation unit sees it.
# This keeps the "upstream source stays untouched" rule intact.
set -eu
SDK=${ANDROID_SDK_ROOT:-/var/minis/workspace/android-sdk}
NDK=$(ls -d "$SDK"/ndk/* 2>/dev/null | head -1)
PRE=$NDK/toolchains/llvm/prebuilt/linux-x86_64
SYS=$PRE/sysroot
UP=${DM_UPSTREAM:-/var/minis/workspace/d2r-upstream}
COMPAT=${DM_COMPAT:-/var/minis/workspace/switch-android-runtime/runtime/compat}
OUT=${1:-/tmp/dm-obj}
mkdir -p "$OUT"

TGT="--target=aarch64-linux-android29 --sysroot=$SYS -resource-dir=$PRE/lib/clang/18"
CF="-O1 -w -fno-stack-protector -fblocks -I$COMPAT/darwin -I$COMPAT"
# Force-include the Darwin shims that upstream does not include itself.
FI="-include $COMPAT/darwin/platform_compat.h"
FILES=${DM_FILES:-"hle_*.c thread.c romfs_validate.c loader.c hle.c"}

cd "$UP/dm/host"
ok=0; bad=0
for f in $FILES; do
    [ -f "$f" ] || continue
    if clang $TGT $CF $FI -I. -I../gfx/nvn/include -c "$f" -o "$OUT/$(basename "$f" .c).o" 2>"$OUT/$(basename "$f" .c).err"; then
        ok=$((ok + 1))
        printf '  OK   %s\n' "$f"
    else
        bad=$((bad + 1))
        reason=$(head -1 "$OUT/$(basename "$f" .c).err")
        printf '  FAIL %-22s %s\n' "$f" "$reason"
    fi
done
printf '\ncompiled_ok=%d failed=%d\n' "$ok" "$bad"
