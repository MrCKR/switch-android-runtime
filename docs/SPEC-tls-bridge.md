> 状态：实施中。

# Spec: Android guest TLS bridge

## 目标与范围

为 Android/Bionic 提供 K18、KTLS、KTLR、KGATE、KRES0、KRES1、KASPECT、KASPECTUI
八个 guest 线程槽的可靠线程隔离存取。当前上游实现依赖 Darwin 私有
`tpidrro_el0` 写入和 pthread TSD 固定布局；真机探针已证明 Android 上读取可用但写入
会触发 SIGILL，因此不能继续复用该假设。

本切片先完成 bridge API、线程隔离测试和 Runtime 自检；下一切片再修改
`patch_nso.py` 的 Android 输出，使改写后的 guest 指令通过可重定位的 Android TLS
入口访问 bridge。真实 D2R 不在本切片加载。

## 归属

主导仓库：`switch-android-runtime`。TLS 槽是所有 Switch guest 的通用运行时能力，
不属于 D2R adapter。

## 现状与依据

- 上游 `patch_nso.py` 把 `mrs tpidr_el0/tpidrro_el0` 和 x18 改写为
  `tpidrro_el0 + key-page offset`；`thread.c` 把每线程 TLS、TLR、KGATE 等值写入
  pthread key。
- 上一切片真机首次 Loader 运行在 `tsd_check()` 处 SIGSEGV，原因是 Bionic 不具备
  Darwin 私有 TSD 固定布局。
- 真机 TPIDRRO 探针结果：`read_ok=1 write_ok=0 signal=4`，即读可执行，写入触发
  `SIGILL`。

## 方案

1. 新增 Android bridge 内部状态：每个宿主 pthread 一个八槽 `uint64_t` 数组，
   通过 pthread key + destructor 管理生命周期；不依赖 Bionic 私有布局，不改变
   Android libc 的 TPIDR_EL0。
2. 暴露 `dm_android_tls_get(slot)` / `dm_android_tls_set(slot, value)`，slot 使用
   稳定的 0–7 枚举，不把 Android pthread key 编号泄漏给 guest。
3. Runtime 启动自检验证当前线程读写和两个 pthread 之间互不串值；保留现有摘要和
   栈/时钟自检。
4. 当前 guest 指令改写仍不在本切片修改。下一切片应让 `patch_nso.py` 的 Android
   模式生成可重定位的 bridge 访问序列，并在加载前完成地址绑定；不得在离线产物中
   硬编码 APK 运行时地址。

## 验收证据

- [x] bridge 在 Android ARM64 下编译并链接
      how: `tools/build_tls_tests.sh /tmp/tls-test`
      want: `ELF64 / AArch64`
      where: 构建输出

- [x] 当前线程八槽读写正确
      how: 在真机执行 `tls_test`
      want: `tls test: PASS`、`slots: PASS`
      where: vivo V2256A stdout

- [x] 两个 pthread 之间槽值隔离
      how: `tls_test` 内部创建子线程并分别写 K18/KTLR
      want: 子线程与主线程各自读回自己的值，退出码 0
      where: vivo V2256A stdout；`tls test: PASS`

- [x] Runtime 启动自检包含 bridge 检查
      how: 启动集成 TLS bridge 的 APK，读取 diagnostics 报告
      want: `compat self-test: PASS`；Vulkan 仍出现 `presented N frames`
      where: `/sdcard/Android/data/org.mrckr.switchruntime/files/diagnostics/vulkan_report.txt`

## 数据边界检查

不读取、不生成、不包含游戏二进制、密钥、shader、资源、存档或 RomFS 派生产物。
本切片只使用固定整数和线程状态：确认。

## 范围外

- 修改 `patch_nso.py` 的 Android 指令改写；
- 真实 guest 执行；
- D2R 版本、patch、adapter；
- Zelda；
- FPS 门槛。

## Evidence

Independent Android ARM64 test:

```text
Class: ELF64
Machine: AArch64
tls test: PASS
slots: PASS
```

The APK containing the same bridge passed its startup self-test and continued
Vulkan presentation on vivo V2256A:

```text
compat self-test: PASS
presented 2040 frames, 55.6 fps (one clear-only frame in flight)
```

The TPIDRRO probe used to choose this design remains:

```text
tpidrro probe: read_ok=1 write_ok=0 before=0 after=0 signal=4
```

This slice completes the bridge storage and validation. It does **not** yet
change `patch_nso.py`; Android guest artifacts still need a follow-up rewrite
mode that emits calls/sequences using this bridge rather than writing
`tpidrro_el0`.

## Thunk ABI (prototype)

An additional entry point exists for the rewrite layer, so the offline rewriter
does not need to know how the bridge stores slots:

```c
uint64_t dm_android_tls_thunk_get(uint64_t slot);
void     dm_android_tls_thunk_set(uint64_t slot, uint64_t value);
```

Verified on vivo V2256A:

```text
tls thunk: PASS
```

These symbols are present in the APK's `libswitch_runtime.so`
(`dm_android_tls_thunk_get` / `dm_android_tls_thunk_set`), so a rewritten guest
image can call them directly once the rewriter binds call sites to them.

### Superseded measurement

An earlier note in this file said `tpidr_el0` write was unusable because a
device probe had failed. That probe never ran (its build failed and a 404 page
was executed instead), so the claim was unfounded. A corrected on-device
measurement gives:

```text
read:  tpidr_el0=0x7708d23000   tpidrro_el0=0
write tpidr_el0:    ok=1  back=0xfeed0000beef  fault_signal=0
write tpidrro_el0:  ok=0  back=0               fault_signal=4
```

`tpidr_el0` is therefore writable, but borrowing it is still impossible: with
the pointer repointed at a private block, libc faults on its first TLS access
in that thread (`fault_signal=11`). Both sysregs are unusable as a guest slot
base, so the rewrite must call into the bridge. See `SPEC-tls-rewrite.md`.

### Why the rewrite is separate work

Upstream's `patch_nso.py` emits `mrs tpidrro_el0` plus a key-page offset load,
relying on Darwin's fixed TSD layout. On Android:

- `tpidrro_el0` read succeeds but returns 0 for the main thread;
- `tpidrro_el0` write raises SIGILL;
- `tpidr_el0` write is likewise not usable for guest slot storage.

So the rewrite output must change, and that change belongs to a dedicated slice
with its own before/after comparison against upstream's `sites.tsv` counts. It
is deliberately **not** done here: doing it silently would change every guest
TLS access site at once, with no separate evidence.
