#!/bin/sh
# Report per-thread CPU time for the runtime process, twice, so a thread that is
# spinning can be told apart from one that is merely parked.
#
# /proc/<tid>/stat puts the command name in parentheses and it may contain
# spaces, so the fields have to be counted from after the closing paren.
PKG=org.mrckr.switchruntime
PID=$(android-shizuku-cli exec "pidof $PKG" 2>/dev/null | grep -o '[0-9]\{3,\}' | head -1)
[ -n "$PID" ] || { echo "not running"; exit 1; }
echo "pid=$PID"
for round in 1 2; do
  android-shizuku-cli exec "for f in /proc/$PID/task/*/stat; do t=\$(cat \$f); rest=\${t#*) }; set -- \$rest; echo \"\$(basename \$(dirname \$f)) \$3 \$14 \$15\"; done" 2>/dev/null \
    | python3 -c "
import json,sys,re
raw=sys.stdin.read()
try:
    j=json.loads(raw[raw.index('{'):raw.rindex('}')+1]); t=j['data']['stdout']
except Exception: t=raw
t=t.replace('\\\\n','\n')
print('--- round $round ---')
for l in t.split('\n'):
    m=re.match(r'^(\d+) (\S+) (\d+) (\d+)', l.strip())
    if m: print('tid=%s state=%s utime=%s stime=%s' % m.groups())
"
  [ "$round" = 1 ] && sleep 4
done
