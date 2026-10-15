# Android Darwin compatibility matrix

Runtime repo evidence for `docs/SPEC-base-framework.md`.

## Host module compile

Baseline command:

```sh
tools/compile_upstream_modules.sh /tmp/dm-obj-final
```

Target: `aarch64-linux-android29`, NDK 27.0.12077973, upstream
`jhaizhou-ops/d2r-mobile@15b837c1e433759d0a46df4d671da3d9c533b710`.

Result:

```text
compiled_ok=31 failed=0
```

The 31 modules are the upstream HLE/host compilation set. The script force-includes
`runtime/compat/darwin/platform_compat.h` so upstream source files remain unchanged;
platform differences are isolated in the runtime compatibility layer.

Important capability notes:

- `clonefile` is implemented as recursive copy on Android. It is correct but not
  APFS copy-on-write; save commit uses Linux `renameat2(RENAME_EXCHANGE)` for the
  atomic swap where available.
- Cross-thread Mach register introspection cannot be reproduced in Android user
  space. The shim returns `KERN_FAILURE`, which makes the optional watchdog dump
  skip other-thread frames rather than claim false register data. The faulting
  thread still uses the signal `ucontext` path.
- AudioToolbox is an AAudio-backed sink adapter. The upstream PCM queue and
  release/event semantics remain in `hle_audio.c`.

## Hash compatibility test

Build command:

```sh
tools/build_compat_tests.sh /tmp/compat-tests
```

Result:

```text
Class: ELF64
Machine: AArch64
```

The same test was also built and run natively on the host before the Android
cross-build:

```text
9 checks, 0 failure(s)
```

Vectors cover SHA-256 empty input, `abc`, a 448-bit message, streamed 7-byte
chunks, one million `a` bytes, 55/56-byte padding boundaries, plus MD5
regression vectors for `abc` and empty input.

## Device execution

The Android AArch64 test binary was transferred to the target phone and run
through Shizuku:

```text
compat hash checks
  ok   sha256("")
  ok   sha256("abc")
  ok   sha256(448-bit message)
  ok   sha256(448-bit, 7-byte chunks)
  ok   sha256(1e6 x 'a')
  ok   sha256(55 x 'a')
  ok   sha256(56 x 'a')
  ok   md5("abc")
  ok   md5("")

9 checks, 0 failure(s)
```

Transfer and execution shape:

```sh
android-shizuku-cli exec "curl -m 120 -sS -o /data/local/tmp/t_hashes http://127.0.0.1:8765/t_hashes"
android-shizuku-cli exec "chmod 755 /data/local/tmp/t_hashes && /data/local/tmp/t_hashes"
```
