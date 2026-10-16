#!/bin/sh
# Push the local tree's changed files to GitHub through the REST API.
#
# github.com:443 is unreachable here while api.github.com answers, so `git push`
# cannot be used.  The Git Data API builds the equivalent commit: upload a blob
# per changed file, build a tree on top of the remote's current tree, create the
# commit, then move the ref.
#
# Only files that differ from the remote tip are uploaded.
set -eu
REPO=${REPO:?}
GITDIR=${GITDIR:?}
BRANCH=${BRANCH:-main}
API=https://api.github.com/repos/$REPO
AUTH="Authorization: Bearer $GITHUBTOKEN"

cd "$GITDIR"

BASE=$(curl -sS -H "$AUTH" "$API/git/ref/heads/$BRANCH" \
  | python3 -c 'import json,sys; print(json.load(sys.stdin)["object"]["sha"])')
BASE_TREE=$(curl -sS -H "$AUTH" "$API/git/commits/$BASE" \
  | python3 -c 'import json,sys; print(json.load(sys.stdin)["tree"]["sha"])')
echo "base $BASE"

# Which paths differ, and how (added/modified/deleted).
git diff --name-status "$BASE" HEAD > /tmp/changes.txt
echo "--- changes ---"; cat /tmp/changes.txt

ENTRIES=""
BODY=/tmp/tree_entries.json
python3 - "$BASE" > "$BODY" <<'PY' &
import json, subprocess, sys
base = sys.argv[1]
out = subprocess.check_output(['git','ls-tree','-r','-z','HEAD']).decode()
want = {}
for item in out.split('\0'):
    if not item: continue
    meta, path = item.split('\t', 1)
    mode, typ, sha = meta.split()
    if typ == 'blob':
        want[path] = (mode, sha)
entries = []
for path, (mode, sha) in want.items():
    entries.append({"path": path, "mode": mode, "type": "blob", "sha": sha})
print(json.dumps({"entries": entries}))
PY
wait

# Upload blobs for added/modified files.
while IFS="$(printf '\t')" read -r status path rest; do
  [ -n "$path" ] || continue
  case "$status" in
    A|M|T)
      git show "HEAD:$path" | base64 | tr -d '\n' > /tmp/one.b64
      python3 -c 'import json; print(json.dumps({"content": open("/tmp/one.b64").read(), "encoding": "base64"}))' > /tmp/blob_body.json
      BLOB=$(curl -sS -X POST -H "$AUTH" -H 'Content-Type: application/json' \
        --data-binary @/tmp/blob_body.json "$API/git/blobs" \
        | python3 -c 'import json,sys; print(json.load(sys.stdin).get("sha",""))')
      [ -n "$BLOB" ] || { echo "blob failed: $path"; exit 1; }
      echo "blob $BLOB  $path"
      ;;
  esac
done < /tmp/changes.txt

# Build the tree: every local blob (mode and sha) layered on the remote tree.
python3 - "$BASE_TREE" <<'PY' > /tmp/tree_body.json
import json, subprocess, sys
base_tree = sys.argv[1]
out = subprocess.check_output(['git','ls-tree','-r','-z','HEAD']).decode()
tree = []
for item in out.split('\0'):
    if not item: continue
    meta, path = item.split('\t', 1)
    mode, typ, sha = meta.split()
    if typ == 'blob':
        tree.append({"path": path, "mode": mode, "type": "blob", "sha": sha})
print(json.dumps({"base_tree": base_tree, "tree": tree}))
PY
TREE=$(curl -sS -X POST -H "$AUTH" -H 'Content-Type: application/json' \
  --data-binary @/tmp/tree_body.json "$API/git/trees" \
  | python3 -c 'import json,sys; print(json.load(sys.stdin).get("sha",""))')
[ -n "$TREE" ] || { echo "tree failed"; exit 1; }
echo "tree $TREE"

MSG=$(git log -1 --format=%B)
python3 - "$MSG" "$TREE" "$BASE" <<'PY' > /tmp/commit_body.json
import json, sys
print(json.dumps({"message": sys.argv[1].rstrip(), "tree": sys.argv[2], "parents": [sys.argv[3]]}))
PY
COMMIT=$(curl -sS -X POST -H "$AUTH" -H 'Content-Type: application/json' \
  --data-binary @/tmp/commit_body.json "$API/git/commits" \
  | python3 -c 'import json,sys; print(json.load(sys.stdin).get("sha",""))')
[ -n "$COMMIT" ] || { echo "commit failed"; exit 1; }
echo "commit $COMMIT"

curl -sS -X PATCH -H "$AUTH" -H 'Content-Type: application/json' \
  --data "{\"sha\":\"$COMMIT\",\"force\":false}" "$API/git/refs/heads/$BRANCH" \
  | python3 -c 'import json,sys; d=json.load(sys.stdin); print("ref ->", d.get("object",{}).get("sha","FAILED"), d.get("message",""))'
