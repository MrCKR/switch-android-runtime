#!/bin/sh
# Dump device logs for the runtime without the harness' own command lines leaking
# into the grep (each android-shizuku-cli invocation logs its command, and that
# text contains the very patterns being searched for).
R=/tmp/logs
mkdir -p "$R"
android-shizuku-cli exec "logcat -d > $R/full.txt 2>/dev/null; ps -A -o PID,UTIME,ARGS 2>/dev/null | grep -a switchruntime > $R/ps.txt; wc -l $R/full.txt" >/dev/null 2>&1
android-shizuku-cli exec "cat $R/full.txt" 2>/dev/null | python3 - "$1" <<'PY'
import json, re, sys
raw = sys.stdin.read()
try:
    j = json.loads(raw[raw.index('{'):raw.rindex('}') + 1])
    txt = j['data']['stdout']
except Exception:
    txt = raw
# The harness logs every command it runs; those lines are noise.
lines = [l for l in txt.split('\n') if 'ShellExecDiag' not in l and 'minis-exit' not in l]
pat = re.compile(sys.argv[1]) if len(sys.argv) > 1 else None
out = [l for l in lines if pat.search(l)] if pat else lines
print('\n'.join(out[-80:]) if out else '(no matching lines)')
PY
