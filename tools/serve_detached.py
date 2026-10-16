#!/usr/bin/env python3
"""Serve a directory over HTTP as a detached process.

Each shell_execute is its own process group, and a plain ``python3 -m
http.server &`` dies with it — which is how an earlier run ended up with the
device pulling a file from a stale server on a reused port.  Forking twice and
calling setsid() leaves the server parented to init, so it survives the shell.
"""
import os
import sys

here = sys.argv[1]
port = int(sys.argv[2])

pid = os.fork()
if pid > 0:
    print(f"server pid {pid} on {port} serving {here}")
    sys.exit(0)
os.setsid()
pid = os.fork()
if pid > 0:
    sys.exit(0)

os.chdir(here)
devnull = os.open(os.devnull, os.O_RDWR)
os.dup2(devnull, 0)
os.dup2(devnull, 1)
os.dup2(devnull, 2)

from http.server import HTTPServer, SimpleHTTPRequestHandler  # noqa: E402
HTTPServer(("0.0.0.0", port), SimpleHTTPRequestHandler).serve_forever()
