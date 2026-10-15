> 状态：实施中，承接 `SPEC-runtime-apk-compat.md` 的后续垂直切片。

# Spec: synthetic ARM64 module loader test

## 目标与范围

验证上游 `dm/host/loader.c` 在 Android ARM64 目标上可以加载一个**不含游戏内容**的合成
NSO 风格镜像：读取 fixups 元数据、映射镜像、解析 MOD0 动态表、应用 AArch64
`RELATIVE` / `ABS64` / `GLOB_DAT` 重定位、解析已定义符号，并调用合成模块的入口函数。

本切片不加载 D2R 文件，不实现真实 D2R patch，不接入 Zelda，不把测试镜像当作游戏资源。

## 归属

主导仓库：`switch-android-runtime`。Loader、Android 构建入口和测试工具不需要知道 D2R。
上游 `loader.c` 保持不改；测试通过最小宿主 stub 提供 HLE/diagnostic 依赖。

## 方案

- 生成一个由脚本构造的合成 patched image 和 fixups TSV；镜像包含 MOD0、动态表、
  符号表、字符串表、三种 AArch64 relocation，以及 `mov w0,#42; ret` 入口；
- 编译上游 `loader.c` 与兼容层为 Android ARM64 可执行测试程序；
- 测试宿主提供 `gate_register`、`resolve_import`、计时、诊断和 force/HLE 计数 stub；
- 调用 `load_modules()` 后检查：模块数量、`module_sym("answer")`、RELATIVE 槽、
  ABS64/GLOB_DAT 槽、外部导入解析槽；调用 `run_module_inits()` 检查返回值 42；
- 同一测试先在主机交叉编译，再推送到 vivo 真机执行。

## 验收证据

- [x] 合成镜像只由测试脚本生成，不含游戏文件
      how: `tools/build_loader_test.sh /tmp/loader-test`
      want: 生成 `synthetic.patched.bin`（65536 bytes）、fixups/gates TSV 与 AArch64 测试程序
      where: `/tmp/loader-test/`；`tools/gen_synthetic_module.py`

- [x] Loader 测试程序在 Android ARM64 编译
      how: `tools/build_loader_test.sh /tmp/loader-test`
      want: `Class: ELF64`、`Machine: AArch64`
      where: 构建输出；`/tmp/loader-test/loader_test`

- [x] 真机加载、重定位、入口调用通过
      how: 推送测试目录到 `/data/local/tmp/loader-test` 后执行测试程序
      want: `loader test: PASS`；`modules=1`；RELATIVE、ABS64/GLOB_DAT 结果正确；`entry=42`；退出码 0
      where: vivo V2256A stdout/stderr；本次输出记录见下方

## 数据边界检查

- 镜像由固定测试脚本生成，不来自游戏文件：确认。
- 不含 ROM、NSP、XCI、密钥、游戏 shader、资源或存档：确认。

## 范围外

- 真实 D2R NSO/NSP 文件；
- D2R 版本识别、patch、gate 清单和资源挂载；
- APK 内 Loader JNI 入口；
- NVN/Vulkan 图形和音频游戏闭环。

## Evidence

Build output:

```text
Class: ELF64
Machine: AArch64
synthetic loader test: /tmp/loader-test/loader_test
```

Target device output (vivo V2256A):

```text
loader test: PASS
modules=1 relative=0x731e02b000 absolute=0x731e02b004 imported=0x123456789abcdef0 entry=42
rc=0
[dm] synthetic: reloc relative=1 abs/glob=2 jump=0
```

The first run intentionally exposed a real Android boundary: upstream's
`tsd_check()` reads Darwin's private `tpidrro_el0` pthread TSD layout and
segfaulted on Bionic. The Android test source now skips only that Darwin-private
probe, while keeping mapping, MOD0 parsing, symbol hashing, all tested
relocations, and entry execution intact. A real Android TLS bridge is a
follow-up before loading guest code that depends on Switch TLS slots.
