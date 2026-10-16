# Handoff — running Diablo II: Resurrected on Android

Written 2026-10-16 at runtime `9e2eb9d` / adapter `25731e0`. This is the state of
the work and what to do next. Read it before touching anything.

## Where things stand, in one paragraph

A user-supplied Switch copy of D2R 1.0.34 now boots on an Android phone: the
guest loader maps `main`/`sdk`/`subsdk0`, the Android TLS rewrite holds, the full
entry chain runs to `nnMain`, the guest creates its own directories and writes
its own log files, and Vulkan presents frames. Then the guest stops: `nnMain`
returns, every worker thread calls ExitThread itself, and no guest thread is left
alive. The guest has run the entire path it can currently run. What is missing is
not a bug to find but work not yet written — listed under "What is actually left".

## Progress this session

| Was | Now |
| --- | --- |
| Adapter archive never linked into `libswitch_runtime.so` | Linked (`--whole-archive`); APK grew 933 KB → 1.4 MB |
| 22 host symbols unresolved → `UnsatisfiedLinkError` at `System.loadLibrary` | Defined in `runtime/host/android_host_contract.c`; **build fails instead of dlopen** thanks to `--no-undefined` |
| xattr failure treated as file-operation failure | `note_guest_name()`; read side accepts `EOPNOTSUPP`/`ENOTSUP` |
| Writable mounts on external storage (`mkdir(2)` → `EOPNOTSUPP`) | `save`/`temp` moved to internal storage (`getFilesDir()`) |
| Guest made no filesystem calls at all | Guest creates `Diablo II/GameLogs/`, opens and writes `D2261016.txt` and `blz-log.txt` in a steady 1 Hz loop; `失败` count 0 |

## Three defects worth remembering

Each was found on a real device, and none of them is in the upstream code.

### 1. A shared object that links is not a shared object that loads

Linking the adapter with plain `-shared` succeeded while leaving 22 symbols
undefined, because a shared object is permitted to carry them. Android's
`dlopen` is strict, so the app died at `System.loadLibrary` with

```
UnsatisfiedLinkError: cannot locate symbol "g_dm_frz_want" referenced by libswitch_runtime.so
```

The earlier "contract is complete" result was measured with a linker that does
not enforce resolution. `tools/link_native_arm64.sh` now links with
`-Wl,--no-undefined` (`--allow-shlib-undefined` exempts platform libraries), and
the production build goes through that script. **Do not bypass it.**

### 2. Extension attributes are unavailable to an Android app in its own data dir

`hle_fs.c` records the guest's original name in the `org.d2rmobile.guest-name`
extended attribute. On Darwin that always works. Under Android's SELinux policy
the app cannot write `user.*` xattrs in its own data directory, so `setxattr`
returns `EOPNOTSUPP` (95) rather than `ENOATTR`.

Two bugs followed from treating that as failure:

- **Write side.** `CreateFile` and `CreateDirectory` had *already succeeded* when
  `set_guest_name` failed, yet reported failure and rolled back. The guest
  concluded the filesystem was read-only and stopped right after `Diablo II`.
- **Read side, worse.** `guest_name()` had `if (errno != ENOATTR) return 0;`, so
  *every* path lookup failed and directories looked empty.

Fixed by `note_guest_name()` (warn once, carry on) and by treating
`EOPNOTSUPP`/`ENOTSUP` as "the physical name is the guest name".

### 3. External storage refuses plain `mkdir(2)`

`/storage/emulated/0/...` is served by FUSE under scoped storage; `mkdir(2)`
returns `EOPNOTSUPP`. `save` and `temp` now live on internal storage. The RomFS
stays where it was pushed — it is only ever read, and moving 5 GB is not worth it.

## What the guest actually does now

```
fs 挂载 $: → /storage/emulated/0/Android/data/org.mrckr.switchruntime/files/romfs（只读）
fs 挂载 ^: → /data/user/0/org.mrckr.switchruntime/files/temp
...
[0x1001] svc 0x18 …            wait for each worker, close its handle
fs open [788359.07s] ^:/Diablo II/GameLogs/D2261016.txt mode=1 → fd 116
fs open [788359.07s] ^:/Diablo II/GameLogs/D2261016.txt mode=6 → fd 116
fs open [788359.09s] ^:/blz-log.txt mode=6 → fd 120
...                             repeats at 1 Hz, then stops
```

Note what is absent: **not one `$:` path is opened.** The guest never reads the
RomFS, so no game resource has been loaded.

## What is actually left

These are three separate pieces of work, not one bug. In the order I would attack
them:

### A. Find out why the guest never reads the RomFS

This is the most promising lead, because resource loading plausibly sits behind
the same branch that ends the run. The runtime side looks correct — the mount is
set up, the 18 archives were byte-count validated against `romfs_validate.c`, and
no read is even attempted.

Look at the adapter's observation gates and its address decisions. The
initialization path being taken is the minimal one. `docs/CONTRACT.md` in the
adapter repo lists ten symbols (`dm_cur_tick`, `dm_diag_*`, `dm_freeze_frame`,
`dm_request_save`, `dm_quiet_difficulty_msg`, `dm_fs_commit_count`,
`dm_ingame_now`, `dm_loadscreen_up`) that are the adapter's migration debt and
are currently weak stubs on the runtime side. Those stubs returning "nothing is
happening" may be exactly why the guest sees no reason to continue.

### B. Make the adapter's game logic take effect

The adapter's 39 modules compile and link, but whether its gates fire is not
established. Instrument that rather than guessing: the gate tables
(`main.gates.tsv`, `sdk.gates.tsv`) are on the device, and `runtime/host/hle_target.c`
holds the gate machinery (`gate_obs_set` / `gate_obs_present` from the contract).
`loader.c` registers them; look for the `guest gates registered` line in the log.

Be warned: `diag.c` (1967 lines of D2R content) still lives in the runtime's host
directory, so the runtime is not yet D2R-free. `CONTRACT.md` records this as
known debt with the reason it cannot move yet — `diag.c` is also the runtime's
tick entry and its gate dispatcher.

### C. Write an NVN graphics backend

**Upstream has no such thing.** The only graphics backend is Metal
(`dm/gfx/metal/*.m`, `nvn_mtl.h`, `hle_nvn.c`). Even once the guest enters a
render loop it cannot draw anything until NVN is translated to Vulkan. This is
the largest item and it is a from-scratch implementation.

## How to build, deploy, and observe

### Build

```sh
# 1. the adapter (from a d2r-android-adapter checkout)
DM_RUNTIME=/path/to/switch-android-runtime sh tools/build-android-arm64.sh
#    expect: "adapter android-arm64: compiled ok=39 failed=0"
#            and "seam symbols: 337"

# 2. the runtime + adapter together
cd /path/to/switch-android-runtime
./tools/build_native_arm64.sh          # finds ../d2r-adapter-x by default,
                                       # or set DM_ADAPTER=/path/to/adapter
```

The build compiles for `aarch64-linux-android29` with the NDK sysroot but uses
the **host** clang (not the NDK's) — see the comment in `build_native_arm64.sh`
about `-resource-dir`; pointing it at the NDK's clang 18 mixes a clang 19
frontend with clang 18's `arm_neon.h` and breaks anything using NEON.

The NDK's `llvm-ar` and `ld.lld` are x86_64 binaries and cannot run on an arm64
host. `tools/build_native_arm64.sh` wraps `ld.lld` through `qemu-x86_64`, and the
adapter script uses the system `ar`.

### Package

Gradle only packages dex, resources, and `jniLibs` here — CMake is not usable
(the NDK's `cmake`/`ld.lld` are x86_64). `externalNativeBuild` is deliberately
removed so Gradle cannot silently reuse a stale `.so`.

```sh
echo "sdk.dir=/path/to/android-sdk" > local.properties
TMPDIR=/tmp/gradle-tmp ANDROID_HOME=/path/to/android-sdk \
  GRADLE_OPTS='-Dorg.gradle.jvmargs=-Xmx1500m -Dorg.gradle.daemon=false' \
  gradle assembleDebug
```

### Deploy

```sh
cd <apk dir>
python3 /path/to/runtime/tools/serve_detached.py . 8999
# then, on the device, fetch to /data/local/tmp and install:
#   pm install requires a real path — it cannot install from /sdcard (FUSE)
```

```sh
android-shizuku-cli exec 'curl -sS --max-time 280 -o /data/local/tmp/d.apk http://<sandbox-ip>:8999/app-debug.apk; md5sum /data/local/tmp/d.apk'
android-shizuku-cli exec 'pm install -r -d -g --user 0 /data/local/tmp/d.apk'
```

**Check the md5 on both sides every time.** A leftover `http.server` from an
earlier session holds the port and serves a stale file; the size mismatch looks
exactly like a truncated transfer. `python3 -m http.server &` does not survive the
shell that started it — that is why `serve_detached.py` forks twice and calls
`setsid`.

### Run and observe

```sh
android-shizuku-cli exec 'logcat -G 16M 2>/dev/null; logcat -c 2>/dev/null; \
  am force-stop org.mrckr.switchruntime; \
  am start -W --user 0 -n org.mrckr.switchruntime/.MainActivity >/dev/null 2>&1; \
  sleep 20; logcat -d -v brief > /data/local/tmp/dl.txt'
```

Raise the logcat buffer first. The default is small enough that Minis' own
`Minis.Perf` logging pushes the game's log out, and you end up staring at a log
with no game lines in it.

Device quirks that cost time: there is no `/tmp` (that is the sandbox path — use
`/data/local/tmp`), `toybox` has no `awk`, and `toybox grep` needs `-a` for
binary logs. `android-shizuku-cli exec` returning JSON means `> file` saves the
envelope, not the log — unwrap `.data.stdout`.

## Debugging tools, and how to read them

`DM_SVC=1`, `DM_THR=1`, `DM_FS=1`, `DM_SAMPLE=<seconds>` are set in
`MainActivity.java` and read via `getenv` from the native side.

- **`hl=`** is the guest thread's last host call, kept in a thread-local by
  `DM_HLE_TRACE` (`hle.h`, `hle_trace.c`) and printed by the sampler. **This is
  the single most useful signal**: it names what a thread was doing when the log
  went quiet.
- The sampler reports `in` or `OUTSIDE` before the module offset. This matters.
  It used to print `mod sdk+0x34039078c` for a frame in a host library, because
  the offset is `pc - base` with no check that `pc` is in range — a number that
  sent me hunting for guest code in a place the guest had never been. `OUTSIDE`
  means a host thread, typically parked in `sched_yield`, and is normal.
- `runtime/host/android_host_contract.c` holds the runtime's half of the
  adapter contract plus weak definitions for the adapter's migration debt.

### Two mistakes I made, so you do not repeat them

**"The main thread is deadlocked in WaitSynchronization."** It was not. The main
thread sits between `svc 0x1b` (SignalProcessWideKey) and `0x1002`'s `svc 0x1a`
(WaitProcessWideKeyAtomic) with a changing timestamp — D2R's render task graph
"wait for everything to finish" is pure polling plus yield (`main+0x15ee898`).
Polling is not deadlock.

**"The guest is stuck."** It was not. What settled it: check whether
`/proc/<pid>/task/*/stat` utime is advancing, and whether any **guest** threads
exist at all. On the final run every thread was ART/binder/hwui — the guest had
finished. The 2000-tick thread I had been staring at was ART's JIT compiler.

The general rule: a sample that repeats the same PC tells you where a thread is,
not whether the program is making progress. Read CPU time and thread inventory
alongside it.

## Reproducing this from scratch

Everything below is needed; the guest will not boot without all of it.

1. **Get the game files from your own copy.** The RomFS is extracted with nstool
   using the update ticket, the base ticket, and `--basenca` pointing at the base
   program NCA — all three are required, or nstool reports
   `Cannot determine RomFs from base nca`. Extraction yields 24 archives; 18 are
   required and were byte-count validated.
2. **Rewrite the modules for Android TLS.** `tools/androidize_nso.py` keeps the
   Apple five-instruction sequence but routes the `mrs tpidrro_el0` through a
   per-module veneer calling `dm_tls_tramp_base`, and makes the key page hold
   slot offsets (`0x4000` apart) rather than `key * 8`. `tpidrro_el0` reads as
   zero on Android and writing it is SIGILL.
3. **Push the modules** to `.../files/guest` as `main`/`sdk`/`subsdk0`
   `.patched.bin` + `.fixups.tsv` + `.gates.tsv`.
4. **Push the 18 RomFS archives** to `.../files/romfs`. `save` and `temp` are
   created by the app on internal storage.

The user supplies their own game files; none of them are in either repository.
That boundary is deliberate and should stay.

## Repository map

`MrCKR/switch-android-runtime` — the host side. HLE, loader, TLS bridge, Vulkan
frame output, the front end. **It exposes interfaces and must not learn about
D2R.** The dependency direction is adapter → runtime.

`MrCKR/d2r-android-adapter` — 39 D2R modules, gate tables, the contract
document. Builds to `libd2r_adapter.a`, which links into the runtime's shared
object.

`docs/CONTRACT.md` in the adapter repo is the authority on which symbols belong
to which side. Read it before moving anything across the boundary.

## Known debt, stated plainly

- `diag.c` still in the runtime's host tree; ten seam symbols still weak stubs.
  `CONTRACT.md` records why it cannot move yet.
- `runtime/host/android_unwind_stub.c` aborts if anything ever throws in the C++
  bridge. Nothing should — there is no unwinder in the NDK sysroot for this ABI,
  and nothing throws — but it is a stub, not a solution.
- The APK is `debug`, signed with the debug key. Fine for testing, not for
  distribution.
