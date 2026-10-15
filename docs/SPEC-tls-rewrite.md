> 状态：完成（合成镜像 + Android ARM64 真机验收通过）。

# Spec: Android guest TLS instruction rewrite

## 目标与范围

让 `patch_nso.py` 在 Android 模式下生成可用的 guest TLS 访问序列，使改写后的
guest 指令能读写 K18/KTLS/KTLR/KGATE/KRES0/KRES1/KASPECT/KASPECTUI 八个槽。

本切片**不**产生真实 D2R 产物，不加载游戏文件；先在合成镜像上验证改写正确性，
再用它跑通带 TLS 访问的合成模块。

## 归属

主导仓库：`switch-android-runtime`。改写器与 TLS 访问序列是通用运行时能力。

## 现状与依据

上游 `patch_nso.py` 的槽访问依赖 Apple 私有机制：

```python
MRS_TPIDRRO(t)                      # mrs tN, tpidrro_el0
emit_slot_addr(tN, tmp, kind)        # 键页: ADRP + LDR key索引
LDRX(value_reg, tN)                  # 读槽
```

即「sysreg 直接给出槽基址，槽基址再加 key-page 偏移」。真机实测（vivo V2256A）：

```text
read:  tpidr_el0=0x7708d23000   tpidrro_el0=0
write tpidr_el0:    ok=1  back=0xfeed0000beef  fault_signal=0
write tpidrro_el0:  ok=0  back=0               fault_signal=4   (SIGILL)
```

TPIDR_EL0 虽然可写，但它是 Bionic 自己的 TLS 指针，借用后果实测：

```text
baseline:   tpidr=0x77aaa40000  errno=22  getspecific=0xabc
repurposed: survived=0  fault_signal=11
restored:   tpidr=0x77aaa40000
```

把 TPIDR_EL0 指向私有块后，同一线程里 libc 的第一次 TLS 访问即段错误。
**结论：两个 sysreg 都不能作为 guest 槽基址，Apple 的内联 sysreg 方案在 Android 上不可用。**

## 方案

采用函数调用方案，复用已实现的 thunk ABI（`SPEC-tls-bridge.md`）：

```c
uint64_t dm_android_tls_thunk_get(uint64_t slot);
void     dm_android_tls_thunk_set(uint64_t slot, uint64_t value);
```

改写规则：

1. `mrs tN, tpidr_el0|tpidrro_el0`（读槽）→ 调用 `thunk_get(slot)`，结果落到原目标寄存器；
2. `msr tpidr_el0, tN`（写 KTLS）→ 调用 `thunk_set(slot, tN)`；
3. x18 读写 → 按 key 映射到 K18 槽，同样经 thunk；
4. 调用前必须按 ABI 保存会被破坏的寄存器（thunk 是 C 函数，x0–x18 为调用者保存），
   且不得污染 NZCV 语义之外的状态；槽访问点位于任意函数中间，因此需要保存/恢复
   调用者保存寄存器，而不只是 x16/x17。

代价与取舍：相比 Apple 的 5 条内联指令，thunk 调用需要保存更多寄存器。
先保证正确性，性能优化留到有真实帧数据之后（列入「范围外」）。
地址绑定：thunk 地址在加载时由运行时提供，不能硬编码进离线产物；
离线产物只记录「此处需要 thunk，slot = N」，由加载器在映射后打补丁。

## 验收证据

- [x] 改写器在 Android 模式下可运行并产出合成产物
      how: `tools/build_rewrite_test.sh /tmp/rewrite-test`
      want: 生成 `synthetic.android.bin` 与 `synthetic.android.fixups.tsv`；输出 `rewritten {'tpidr_w': 1, 'tpidr_r': 1, 'tpidrro_r': 1} sites 3 fixups 3`
      where: `/tmp/tls-rewrite-test/`

- [x] 合成镜像中的槽访问改写正确
      how: 真机执行 `tls_rewrite_test synthetic.android.bin synthetic.android.fixups.tsv`
      want: `bound_fixups=3`、写入 KTLS 后读回 `0xabcd1234`、`tls rewrite test: PASS`、退出码 0
      where: vivo V2256A stdout

- [x] 改写点计数与测试镜像一致
      how: 对照合成 guest 中 1 个 `tpidr_w`、1 个 `tpidr_r`、1 个 `tpidrro_r`
      want: 三类各 1 个；三条 load-time thunk fixup；无遗漏、无重复
      where: `synthetic.android.sites.tsv` 与 `synthetic.android.fixups.tsv`

- [x] APK 启动自检不回归
      how: 启动现有集成 APK，读取 diagnostics 报告
      want: `compat self-test: PASS`
      where: 应用 diagnostics 报告

## 数据边界检查

只用合成镜像与固定常量；不生成、不读取、不提交游戏二进制、密钥、shader、资源、存档。
确认。

## 范围外

- 真实 D2R 模块与 adapter；
- thunk 调用性能优化（后续以真实帧数据为依据）；
- Zelda；
- FPS 门槛。

## Evidence

The synthetic guest routine contains one site of each supported form:

```text
tpidr_w    1
tpidr_r    1
tpidrro_r 1
fixups     3
```

The rewrite replaces each site with a branch to a local patch block. Each block
calls a register-preserving trampoline; load-time binding patches the placeholder
calls to the native trampoline through a near veneer when the native library is
outside AArch64 BL range.

The final vivo V2256A result:

```text
bound_fixups=3 sites_ran=1 stored=0xabcd1234 read=0xabcd1234
tls rewrite test: PASS
rc=0
```

The guest routine saves/restores its ABI callee-saved registers and return
address, and the trampoline preserves the live register state around the C
thunk. The test therefore validates the actual rewritten instruction path, not
just direct calls to the TLS API.

The Android APK self-test remains green after the bridge/thunk integration:

```text
compat self-test: PASS
```
