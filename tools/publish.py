#!/usr/bin/env python3
"""Publish the working tree on top of the remote branch, via the REST API.

github.com:443 is unreachable from this sandbox while api.github.com is not, so
git's own transport cannot be used. This creates one commit per run containing
every path whose content differs from the remote branch head.

Usage: publish.py <repo_dir> <message_file> [branch]
Reads the token from $GITHUBTOKEN and never prints it.
"""
import base64
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request

TOKEN = os.environ.get("GITHUBTOKEN")
if not TOKEN:
    sys.exit("GITHUBTOKEN not set")

REPO_DIR = sys.argv[1]
MSG_FILE = sys.argv[2]
BRANCH = sys.argv[3] if len(sys.argv) > 3 else "main"
API = "https://api.github.com"


def git(*args, binary=False):
    return subprocess.check_output(["git", "-C", REPO_DIR, *args],
                                   text=not binary)


def request(method, path, payload=None):
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(API + path, data=data, method=method)
    req.add_header("Authorization", f"Bearer {TOKEN}")
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("User-Agent", "minis-publish")
    if data:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=180) as r:
            body = r.read()
            return json.loads(body) if body else {}
    except urllib.error.HTTPError as e:
        raise SystemExit(f"{method} {path} -> {e.code}\n{e.read().decode(errors='replace')}")


slug = git("remote", "get-url", "origin").strip().split("github.com/")[-1].removesuffix(".git")
head = request("GET", f"/repos/{slug}/git/ref/heads/{BRANCH}")
remote_sha = head["object"]["sha"]
remote_commit = request("GET", f"/repos/{slug}/git/commits/{remote_sha}")
base_tree = remote_commit["tree"]["sha"]
print("repo:", slug, "\nremote head:", remote_sha)

remote_tree = request("GET", f"/repos/{slug}/git/trees/{base_tree}?recursive=1")
remote = {e["path"]: e["sha"] for e in remote_tree["tree"] if e["type"] == "blob"}

entries = []
for line in git("ls-tree", "-r", "HEAD").splitlines():
    meta, path = line.split("\t", 1)
    mode, _otype, osha = meta.split()[:3]
    if remote.get(path) == osha:
        continue
    content = git("cat-file", "blob", osha, binary=True)
    blob = request("POST", f"/repos/{slug}/git/blobs",
                   {"content": base64.b64encode(content).decode(), "encoding": "base64"})
    entries.append({"path": path, "mode": mode, "type": "blob", "sha": blob["sha"]})
    print(f"  changed {path} ({len(content)} bytes)")

for path in sorted(set(remote) - set(
        l.split("\t", 1)[1] for l in git("ls-tree", "-r", "HEAD").splitlines())):
    entries.append({"path": path, "mode": "100644", "type": "blob", "sha": None})
    print(f"  deleted {path}")

if not entries:
    print("nothing to publish")
    sys.exit(0)

message = open(MSG_FILE).read().rstrip()
tree = request("POST", f"/repos/{slug}/git/trees",
               {"base_tree": base_tree, "tree": entries})
commit = request("POST", f"/repos/{slug}/git/commits",
                 {"message": message, "tree": tree["sha"], "parents": [remote_sha]})
request("PATCH", f"/repos/{slug}/git/refs/heads/{BRANCH}",
        {"sha": commit["sha"], "force": False})
print("\npublished", BRANCH, "->", commit["sha"])
