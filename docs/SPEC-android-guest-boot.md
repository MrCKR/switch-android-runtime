# Android guest boot and real D2R staging

## 目标与范围

对应 Phase 2 收尾 / Phase 4 前置，主导仓库为 `switch-android-runtime`。
目标是把通用 Android loader、HLE、TLS、gate 机制接入 APK，并建立真实 D2R
模块的本地 staging/版本拒绝路径。第一验收是合成 guest 真机启动；第二验收是
用户自有 1.0.34 模块通过版本校验并进入 guest bootstrap。真实标题画面另依赖
NVN→Vulkan，不把“loader 成功”冒充“游戏已运行”。

## 归属

通用 loader、线程、TLS、HLE 门和 Android 文件前端属于 runtime；D2R 专用
39 个 `dm_*.c`、版本地址、生成头和离线 patch 规则属于 adapter。游戏文件、
密钥、NSP/XCI、NSO 和任何从它们生成的二进制不进入 Git。

## 现状与依据

- runtime 远程 `6f849d8` 已通过 TLS 改写真机验收。
- 真机 guest 地址空间探针通过：5GB 预留、4GB heap backing、3797MB pool。
- 用户设备中发现 1.0.33 layeredfs zip 和 1.0.34 offline-mod RAR；后者包含真实
  `main`，build ID 为 `db0507f36b84200ac05f75d76ed5026f`，与现有 patch 规则匹配。
- 1.0.34 main 的离线 MOD 已经预应用部分 CUTS（例如离线窗口的 new bytes），
  因此 patcher 必须明确支持“原字节或已应用新字节”的本地输入模式，不能静默
  覆盖未知字节。
- 当前 APK 只含 Vulkan clear/present 和 compat self-test；上游 loader/HLE 尚未
  编入共享库。上游 generic host 代码的 Android 交叉编译可达到 69/73，剩下
  3 个用户生成头和 1 个 crash context ABI 缺口。

## 方案

1. 将 generic host 源码放在 runtime 的 Android host 目录，Android-only 的
   `entry.S` 去掉 Darwin 前导下划线；不把 D2R `dm_*.c` 与 `diag.c` 带入 runtime。
2. 把平台缺口集中在 compat：SHA1、`st_mtimespec`、`CLOCK_UPTIME_RAW`、
   `sysctl` fallback、Linux AArch64 `sigcontext` 映射。host C 代码只引用宏，不
   修改上游源文件。
3. 用 typed host pthread TLS API 保留 Bionic key 语义；guest sysreg 仍只走
   register-preserving thunk，禁止写 TPIDR_EL0。
4. 让 Android bootstrap 接受显式 staging 目录，先检查 manifest、module 名称、
   build ID 和 SHA-256，再调用 `load_modules`；不接受原始 NSO 直接进入 loader。
5. 命名 section 用 ELF `dmsec_*` bounds 接入；注册表保留给运行时数据段，三张
   HLE/NVN 表缺失时必须报错而不是静默空表。
6. 真实 1.0.34 的 `sdk`、RomFS 和生成头缺失时，诊断明确指出缺哪些用户文件；
   继续做合成 guest 和通用 HLE 验收，不伪造真实游戏启动成功。

## 验收证据

- `DM_FILES="..." tools/compile_upstream_modules.sh`：generic Android host
  编译矩阵无 runtime 语义错误；生成头缺失只在 adapter 侧报告。
- runtime host standalone link：无 undefined generic symbols，且 `readelf` 确认
  entry/gate/unimplemented stub 符号和 8-byte stub 间距。
- APK 真机报告：`guest metadata accepted`、`module mapped`、`relocations complete`、
  `gates registered`、`guest entry returned`，并保留 Vulkan present 帧数。
- 对用户 1.0.34 main：build ID、原始 SHA-256、每条 CUT/OBS 原字节或“已应用”
  状态逐项记录；任意其他字节拒绝。
- 所有生成的 patch image、NSP/XCI/NSO、RomFS、密钥、生成头只在 app-private 或
  沙箱临时目录，`git status` 不得出现这些文件。

## 数据边界检查

本规格不包含任何游戏二进制、资源、密钥、存档或提取产物；文中的 build ID、
文件格式和偏移只作为版本元数据与测试依据。

## 范围外

本切片不实现完整 NVN shader/pipeline、D2R 39 个模块迁移、用户 SAF UI、真实
标题画面或离线游戏；这些在 generic guest boot 通过后继续做。

## 未决问题

1. 用户下载目录没有发现 `prod.keys`；NSP 可能无法在本地解密以提取 sdk/RomFS。
2. 1.0.34 RAR 的 main 已可读取，但 patch archive 是否包含 sdk 尚未确认。
3. generic HLE 中的 graphics front 与 Android Vulkan runtime 的所有权需要在首次
   standalone link 后冻结。
