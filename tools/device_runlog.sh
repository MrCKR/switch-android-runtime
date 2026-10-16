#!/bin/sh
# Start the app and show everything the system logged for it, including the
# crash dump.  The guest is big enough that start-up diagnostics only appear in
# logcat, and the app's own tag is "dmhost".
PKG=org.mrckr.switchruntime
android-shizuku-cli exec "logcat -c 2>/dev/null; am start -n $PKG/.MainActivity >/dev/null 2>&1; sleep 12; logcat -d -v brief > /tmp/logs/run.txt 2>/dev/null; wc -l /tmp/logs/run.txt" >/dev/null 2>&1
android-shizuku-cli exec "cat /tmp/logs/run.txt" 2>/dev/null | python3 - <<'PY'
import json
import sys
raw = sys.stdin.read()
try:
    j = json.loads(raw[raw.index('{'):raw.rindex('}') + 1])
    txt = j['data']['stdout']
except Exception:
    txt = raw
keep = []
for l in txt.split('\n'):
    if 'ShellExecDiag' in l or 'minis-exit' in l or 'logcat -d' in l:
        continue
    if any(k in l for k in ('dmhost', 'switchruntime', 'SwitchRuntime', 'AndroidRuntime',
                            'FATAL', 'DEBUG', 'libc', 'tombstone', 'ActivityManager')):
        keep.append(l)
print('\n'.join(keep[-70:]) if keep else '(nothing)')
PY
