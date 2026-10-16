#!/bin/sh
# Print the device log with the harness' own command lines removed.
#
# Every android-shizuku-cli call is itself logged (ShellExecDiag), and the logged
# text contains whatever pattern was being grepped for — so a naive grep returns
# its own command line and the output looks like noise.  Fetch the log once, then
# filter locally.
FILTER=${1:-.}
android-shizuku-cli exec 'logcat -d -v brief > /data/local/tmp/dmlog.txt 2>/dev/null; wc -l < /data/local/tmp/dmlog.txt' >/dev/null 2>&1
android-shizuku-cli exec 'cat /data/local/tmp/dmlog.txt' 2>/dev/null | python3 - "$FILTER" <<'PY'
import json, re, sys
raw = sys.stdin.read()
try:
    j = json.loads(raw[raw.index('{'):raw.rindex('}') + 1])
    txt = j['data']['stdout']
except Exception:
    txt = raw
txt = txt.replace('\\n', '\n').replace('\\/', '/').replace('\\"', '"')
noise = ('ShellExecDiag', 'minis-exit', 'logcat -d', 'dmlog.txt', 'tail -', 'toybox grep')
lines = [l for l in txt.split('\n') if not any(k in l for k in noise)]
pat = re.compile(sys.argv[1])
keep = [l for l in lines if pat.search(l)]
print('\n'.join(keep[-60:]) if keep else '(no match)')
PY
