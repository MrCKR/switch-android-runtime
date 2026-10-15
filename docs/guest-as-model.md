# The guest address-space model on Android

**Status:** measured on the target device. `tools/build_as_probe.sh` +
`tests/guest_as_probe.c`, exit 0.

## Why this needed measuring

Upstream `dm/host/hle_svc.c` reserves one 5 GB `PROT_NONE` block and partitions
it itself, then reports an application memory pool to the guest of
`3285 MB + 512 MB` and hands the whole block to the game's allocator. Every one
of those numbers was chosen against **iOS**, and the file says so:

> iOS 上不带「扩展虚拟寻址」权限时一次最多能预留 6 GB（真机实读：6 GB 成、7 GB 不成），
> 所以不能留得更宽
> *(on iOS without the "extended virtual addressing" entitlement a single
> reservation can be at most 6 GB — read on a real device: 6 GB succeeds, 7 GB
> fails — so it cannot be made any wider)*

and the pool size is the Switch 4 GB model's application pool, not an iOS or
Android figure. A reservation strategy that exists to fit under an iOS ceiling,
and a pool size carried over from a console, are both assumptions about a
platform the code has never run on. Reserving address space and being allowed
to *back* it are also different questions, and only one of them was answered by
the upstream comments.

## What was measured

On vivo V2256A / Android 16 / arm64, `tests/guest_as_probe.c`:

```
-- largest single reservation --
   1 GB ... 16 GB  reserve ok      (all succeed)
reserve: at least 5 GB succeeds                ok  largest ok = 16 GB

-- upstream 5 GB reservation and partition layout --
reserve: 5 GB (upstream RESERVE)               ok
  AS    0x722ee00000 +0x140000000
  alias 0x723ee00000 +0x20000000
  stack 0x725ee00000 +0x10000000
  heap  0x726ee00000 +0x100000000
layout: heap end is inside the reservation     ok
layout: partitions do not overlap              ok

-- commit heap region (4096 MB) --
commit: full 4 GB heap region backed           ok  committed 4096 MB of 4096 MB

-- reported application pool (3797 MB) --
pool: fits inside the 4 GB heap region         ok  pool 3797 MB vs heap region 4096 MB
pool: can be committed on its own              ok

guest address-space model probe: PASS
```

## What this settles

1. **Upstream's reservation arithmetic is valid on Android unchanged.** The
   partition offsets and the 5 GB total can be taken as-is; no re-derivation is
   needed.
2. **The iOS 6 GB ceiling does not apply.** The probe reserved 16 GB in a
   single `PROT_NONE` mapping without failing. The constraint that shaped
   upstream's sizing is an iOS entitlement limit, so on Android the reservation
   can be widened later if the heap needs it — the budget is headroom, not a
   wall.
3. **The 4 GB heap region can actually be backed**, not merely reserved:
   mapping it read-write and faulting every page in succeeded for all 4096 MB.
   This matters because the game's `StandardAllocator` receives the whole block
   and commits pages itself; a region that reserved but could not be backed
   would fail later, at the point where the game is least able to cope.
4. **The reported pool stands on its own** even if the block reservation is not
   in play, so the two are not coupled.

## What this does *not* establish

The probe measures the mmap model. It says nothing about:

- whether the game's allocator behaves correctly against a pool this size
  (that needs the game);
- resident-memory consequences of the pool at runtime — upstream's comment
  notes untouched heap pages do not count toward resident memory, which is a
  property of Linux page accounting and should hold, but has not been measured
  under the runtime's own allocation pattern;
- behaviour under memory pressure from other apps, which is where a device with
  less RAM than this one would differ. The probe's device reports
  `heap[0]=11GB`.

## Reproducing

```sh
sh tools/build_as_probe.sh /tmp/dm-as-probe
# deliver /tmp/dm-as-probe/guest_as_probe to the device and run it; exit 0 = PASS
```

See the transfer recipe in the project state file — the device reaches the
sandbox over wlan0, not loopback.
