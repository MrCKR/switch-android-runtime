# Mach-O named sections on ELF

**Status:** mechanism verified on the host toolchain (clang/LLD + binutils).
Not yet wired into the runtime — see "Current gap".

## Why this exists

Upstream reaches into named sections of its own Mach-O executable to find three
tables. There is no Mach-O image on Android, and the compat layer's `getsect.h`
says so — it keeps an explicit registry of named sections instead, and the
comment there claims the runtime "fills it at startup by mapping the files
produced by the offline build stage".

That is a reasonable design. The problem is that it never happened: the
registry has **no producer**.

| Section | Producer | Consumer | Consequence today |
|---|---|---|---|
| `__DATA,swhle` | `hle_table.c` — 68 libc passthrough entries | `hle.c:31` `resolve_import` | every libc import falls through to a named stub |
| `__DATA,swforce` | `HLE_FORCE(...)` macro, `hle.h:42` | `loader.c:127` | no forced take-over runs |
| `__DATA,swnvn` | `hle_nvn.c:69,75` | `hle_nvn.c:87` | NVN has no implementation table |

```
$ rg -n 'dm_compat_register_section' runtime/ app/ tests/
runtime/compat/dm_compat_macho.c:26          <- the definition
runtime/compat/darwin/mach-o/getsect.h:38    <- the declaration
```

Nothing else. Every lookup returns `NULL` and `size = 0`. Because both
consumers guard on the result (`if (!sec) sec = getsectiondata(...)`, then loop
over `secsz`), this fails **silently**: no crash, no message, just the tables
missing. It is the kind of gap that only shows up much later, as
"every libc call is going to a stub" or "NVN is unimplemented" — which would
look like eleven separate problems rather than one.

Worth stating plainly: it is not currently *harmful*. The `swhle` table only
holds libc passthrough and there is no NVN layer yet. It is a latent trap, and
it is cheap to close now.

## A cheaper route than the documented one

The `getsect.h` comment proposes that the runtime reconstruct these tables at
startup from files staged by the offline build. Section data is not needed for
that: the tables are just arrays of `{const char *name; void *fn;}` and could
be built at runtime from a name list.

But the table entries already exist in the compiled objects, correctly
initialised, and the linker can hand them over as a contiguous array. Verified
end to end with upstream's own `hle_table.c`:

### 1. The section attribute survives

```c
#define L(n) __attribute__((used, section("__DATA,swhle"))) \
    static const hle_ent hle_lib_##n = { #n, (void *)n };
```

compiled with `--target=aarch64-linux-android29` produces a real ELF section
literally named `__DATA,swhle`:

```
[ 4] __DATA,swhle      PROGBITS  0000000000000000 0001f0 0003f0 00  WA  0  0  8
[ 5] .rela__DATA,swhle RELA      ...                          I   8  4  8
```

Astute readers will note that the section *name* is not a problem — Intel-style
permission suffixes (`.data.foo` → read-only) do not apply to this name, and
`llvm-nm` shows the entries present with local binding.

### 2. `objcopy` renames it to something a linker script can address

```
objcopy --rename-section '__DATA,swhle=dmsec_swhle,alloc,load,data' in.o out.o
[ 3] dmsec_swhle      PROGBITS  ...  WA  0  0  8
```

Note the flags: **`WA`**, not `A`. The trailing `readonly` in the flag list is
the thing that matters, not the section name — the name keeps its dot and comma
and links fine. Controlled experiment, one variable at a time:

| objcopy flags | section name | link |
|---|---|---|
| `alloc,load,readonly,data` | `.dmsec_swhle` | **FAIL** |
| `alloc,load,data` | `.dmsec_swhle` | **OK** |
| `alloc,load,readonly,data` | `dmsec_swhle` | **FAIL** |

The dotted name — the thing Intel-style permission-suffix parsing would reason
about — is irrelevant here; `readonly` alone is the cause.

This is because the relocation table that follows (`R_AARCH64_RELATIVE`, one
per pointer entry) is applied to the section contents at load time, so a section
holding pointers cannot be read-only.

### 3. `__start_`/`__stop_` symbols give the bounds

A plain `SECTIONS`/`INSERT` linker script entry failed here — LLD rejects both
`INSERT AFTER .data` and `INSERT AFTER .rodata` (`unable to insert ... after`).
The mechanism that works is the implicit start/stop pair, with no linker script
at all:

```c
extern const char __start_dmsec_swhle[] __attribute__((weak));
extern const char __stop_dmsec_swhle[] __attribute__((weak));
```

```
[21] dmsec_swhle  000000000000dea8  0003f0
  68: __stop_dmsec_swhle    GLOBAL PROTECTED  21
  69: __start_dmsec_swhle   GLOBAL PROTECTED  21
```

`PROTECTED` visibility is what makes this work out of a shared object without
per-symbol exports.

### 4. The table's pointers relocate correctly

```
$ readelf -rW libsec.so | head
  Offset     Info             Type                Symbol's Name + Addend
  000000000000dea8  0000000000000403 R_AARCH64_RELATIVE   1995
  000000000000deb8  0000000000000403 R_AARCH64_RELATIVE   1a1c
  ...
$ readelf -lW libsec.so | rg 'dmsec|LOAD|RELRO'
  LOAD  0x001ea8 ... 0x0003f0 RW        <- section lives here
  GNU_RELRO 0x001cf0 ... 0x0001b8 R     <- it is *not* in here
```

Every table entry is a static `R_AARCH64_RELATIVE` relocation, applied at load
time; the section sits in a writable mapping outside `GNU_RELRO`, so application
succeeds. `__start_dmsec_swhle` itself resolves via an `R_AARCH64_RELATIVE` too,
which is correct — a PC-relative address of a local would not have relocated.

### The `-fPIC` red herring

Linking these objects failed with `R_AARCH64_ABS64 cannot be used against local
symbol; recompile with -fPIC`. The message names the wrong fix: `-fPIC` does not
change the relocation type, because the C initialiser `(void *)memcpy` is an
absolute data pointer either way, and the compiler keeps `R_AARCH64_ABS64` in
the object and lets the linker turn it into `R_AARCH64_RELATIVE`. Verified by
dumping the relocation table of the same object built with and without `-fPIC`:
identical. That is why the message points at a setting that changes nothing,
and why the real cause (the `readonly` flag above) is easy to miss.

Worth recording because it cost time: the cause the error names is not the
actual cause, and following it leads nowhere.

## Current gap

The mechanism is proven but **not wired in**. What remains:

1. a build step applying `objcopy --rename-section` to the objects that define
   `swhle` / `swforce` / `swnvn` (three sections, one `objcopy` invocation or a
   pass over the object list);
2. `dm_compat_find_section` gaining a lookup for the three named sections via
   `__start_`/`__stop_`, alongside (not instead of) the registry, so the
   registry stays available for sections that genuinely come from data files;
3. a test asserting a known entry is found and its pointer is callable — a
   non-empty result alone would pass on garbage.

## Reproducing

```sh
NDK=$(ls -d $ANDROID_SDK_ROOT/ndk/* | head -1); PRE=$NDK/toolchains/llvm/prebuilt/linux-x86_64
clang --target=aarch64-linux-android29 --sysroot=$PRE/sysroot \
      -resource-dir=$PRE/lib/clang/18 -O1 -w -fPIC \
      -include <compat>/darwin/platform_compat.h -I<compat>/darwin -I<compat> \
      -c hle_table.c -o ht.o
objcopy --rename-section '__DATA,swhle=dmsec_swhle,alloc,load,data' ht.o ht2.o
clang --target=aarch64-linux-android29 --sysroot=$PRE/sysroot \
      -shared -fPIC -o libsec.so ht2.o useref.c      # useref.c references __start_/__stop_
readelf -SW libsec.so | rg dmsec
readelf -rW libsec.so | rg -m3 R_AARCH64_RELATIVE
```
