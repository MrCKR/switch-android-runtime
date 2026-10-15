> 状态：**草稿，待评审**。未提交、未推送。评审通过后才进入实现（见 dev-spec 的两 turn 门控）。

# Spec: 上游 Apple 专有层到 Android 的基础框架移植

## 目标与范围

把上游 `jhaizhou-ops/d2r-mobile` 中与 Apple 平台耦合的部分，在 `switch-android-runtime`
内替换为 Android 实现，使上游 **Switch 游戏代码、服务层（HLE）、NVN 图形层** 能在
Android ARM64 上编译并运行。

里程碑归属：对应基础框架阶段。**本文不覆盖 D2R 适配器内容，也不覆盖塞尔达**——
后两者见「范围外」。

本文覆盖四块：

1. **Darwin 兼容层**——补齐上游依赖的 Apple 专有头文件与符号；
2. **HLE 服务层**——替换 8 个失败模块中的 Apple 专有调用；
3. **图形后端**——NVN → Vulkan（上游 `nvn_mtl.m` 不复用）；
4. **着色器链**——接上上游在 SPIR-V 处断开的链路。

## 假设

动笔前先亮出本文依赖的前提。其中任何一条不成立，方案都要重来。

1. 上游基线固定在 `15b837c`，允许在移植中局部改动，但改动必须可追溯；
2. 「基础框架」指 runtime 内替代上游 Apple 专有耦合的那一层，不含 D2R 专用内容；
3. 验收在 vivo V2256A 真机上、用**使用者自己合法导出**的游戏文件完成，不进 CI；
4. D2R 适配器不只是下游产物，同时是 runtime 的**测试手段**，两者并行推进，
   不是严格串行；
5. 图形路径必须换 Vulkan 后端，上游 `nvn_mtl.m`（6386 行）零复用；
6. 着色器链在 SPIR-V 处接上，Android 不需要 `spirv-cross` 与 MSL；
7. 塞尔达为后续里程碑，本文不做任何为它预留的设计；
8. 性能**只作为技术路线的约束**，不在本阶段设帧率门槛（见「验收证据」）。

## 归属

**主导仓库：`switch-android-runtime`。**

判据（本项目的归属规则）：这段逻辑需不需要知道「这是 D2R」？
上面四块都不需要——它们对任何 Switch 游戏都成立，所以归 runtime。

需要写清楚的一条边界警告：上游 `dm/host/` 下有 **38 个 `dm_*.c`**
（`dm_lootfilter.c`、`dm_runeword.c`、`dm_shoprefresh.c`、`dm_pages.c`、`dm_vault.c` …），
它们是 D2R 的游戏功能模块，**不属于本文范围，也不得进入 runtime**。
它们归 `d2r-android-adapter`。

## 现状与依据

上游基线钉在 `jhaizhou-ops/d2r-mobile@15b837c1e433759d0a46df4d671da3d9c533b710`
（本地 `15b837c README: list Apple TV 4K ...`）。许可证 MIT，可移植。

### 已实测：可移植边界的真实大小

以下数字是本次在 Android NDK 目标下实际编译得到的，不是估算。

编译口径：`clang --target=aarch64-linux-android29`，NDK sysroot，
`-I runtime/compat/darwin -I runtime/compat`，源在 `dm/host/`。

**宿主层 31 个服务模块 → 23 个零改动通过，8 个失败：**

| 结果 | 文件 |
|---|---|
| 通过（23） | `hle_account hle_charname hle_codec hle_count hle_friends hle_granny hle_guestthread hle_hid hle_misc hle_movie hle_net hle_oe hle_os hle_private_aim hle_sdkinit hle_swkbd hle_table hle_tact hle_target hle_touch hle.c thread.c romfs_validate.c` |
| 失败（8） | 见下表 |

**8 个失败点及实测工作量：**

| 文件 | 行数 | 缺失 | 用量 | 替代难度 |
|---|---|---|---|---|
| `hle_fdstat.c` | 40 | `getdtablesize` | 1 处 | 低，用 `getrlimit(RLIMIT_NOFILE)` |
| `hle_time.c` | 97 | `clock_gettime_nsec_np` | 1 处 | 低，`clock_gettime` 合成 |
| `hle_svc.c` | 252 | `pthread_get_stackaddr_np` | 1 处 | 低，`pthread_attr_getstack` |
| `hle_thread.c` | 671 | `pthread_mach_thread_np` | 2 处 | 低-中，需线程身份映射 |
| `hle_tact.c` / `loader.c` | 396 | `CC_SHA256` | `loader.c` 4 处 | 低-中，已有 `CC_MD5` shim 可照做 |
| `hle_lifecycle.c` | 34 | `dispatch_*` | 7 种 | **中**，GCD 语义需映射到 pthread + timer |
| `hle_fs.c` | 740 | `clonefile` | 11 处 | **中**，APFS 写时复制无对应物，需退化为 copy |
| `hle_audio.c` | 297 | `AudioToolbox` | 整层 | **高**，需换 Android 音频实现，仅 2 个导出函数 |

**图形层接口面：**

- `dm/gfx/nvn/names.txt` 明确列出 **523 个 NVN 函数**；
- `nvn_mtl.m` 是 **6386 行**的 NVN→Metal 后端，Android 不复用；
- `hle_nvn.c` 1384 行，是 NVN 分发与记录层，平台无关部分可复用；
- `nvn_dirty.c` 等图形工具层 C 文件此前已零改动编译通过。

### 已实测：着色器链在 SPIR-V 处断开

这是 Android 路线比 Metal 路线更短的地方，值得单独说明：

`dm/gfx/tools/dmsh_xlate/Program.cs:122` 已经是：

```csharp
var ctx = Translator.CreateContext(0, acc, new TranslationOptions(TargetLanguage.Spirv, TargetApi.Vulkan, flags));
```

即 Ryujinx 翻译器**直接产出 SPIR-V、目标 API 就是 Vulkan**。
上游随后的 `dm/ios/tools/ios_metal.py`（12 行）才把它经 `spirv-cross` 转成 MSL → metallib。

**结论：Android 不需要 `spirv-cross`，也不需要 MSL。** 上游在 `ios_metal.py` 这里接 Apple，
Android 从同一个 SPIR-V 出口直接进 Vulkan。

待验证的风险：该链依赖 .NET 工具 `dmsh_xlate`，而本机无 `dotnet`/`mono`
（`apk search dotnet` 只找到 host/运行库包，非 SDK）。这是本文最大的未决问题。

### 已实测：Android 目标设备的图形能力

（已归档于 `docs/PHASE2.md`，此处仅引用结论，不重述过程）

Adreno 730，Vulkan 1.1.128，extension 90 个，含
`VK_KHR_swapchain`、`maintenance1/2`、`image_format_list`、`timeline_semaphore`、
`descriptor_indexing`；ASTC/ETC2/BC 三种压缩纹理全支持；heap[0] 11310MB；
`multiViewport=0`。

## 方案

### 一、Darwin 兼容层（补齐现有 `runtime/compat/`）

现有 14 个文件已建立（`darwin/mach/*`、`darwin/mach-o/*`、`TargetConditionals.h`、
`libkern/OSCacheControl.h`、`CommonCrypto/CommonDigest.h`、`pthread_compat.h` 及
`dm_compat_*.c`）。沿用已确立的策略：**上游源码零改动**，差异全部收敛在兼容层。

本次需新增：

- `CC_SHA256` 系列（照 `dm_compat_md5.c` 的既有做法）；
- `pthread_mach_thread_np` / `pthread_get_stackaddr_np` 的 shim，
  内部维护 pthread ↔ 平台线程标识的映射；
- `clock_gettime_nsec_np`、`getdtablesize` 的直替。

取舍：`hle_lifecycle.c` 用的 `dispatch_*`（GCD）有两种接法——
(a) 在兼容层实现一个 GCD 子集；(b) 把它从兼容层移出，作为 Android 侧的
平台实现单独写。倾向 (b)：GCD 是**平台能力**而非头文件缺失，塞进 `darwin/`
会掩盖「Android 用的是另一套调度」这个事实。

### 二、HLE 服务层（8 个失败模块）

按上表逐点替换，全部保持上游调用点不变。唯一例外是 `hle_audio.c`：
`AudioToolbox` 是整层替换而非补符号，倾向让它在 Android 侧重新实现
（导出函数仍只有原有两个，边界干净）。

### 三、图形后端：NVN → Vulkan（本文最大工作量）

**采用「运行简化、允许重新设计」的方针**，不照搬 `nvn_mtl.m` 的对象模型。

上游 Metal 后端 6386 行，其结构大量服务于 Metal 的特性（命令缓冲区、
编码器、资源堆）。Vulkan 的对应物不同，直译只会得到一个两头不讨好的实现。

推荐路径：

1. 以 `hle_nvn.c`（记录/分发层，平台无关）为骨架，保留 NVN 语义；
2. 后端只实现 D2R 实际触发的 NVN 函数子集——**不追求 523 个全覆盖**，
   未实现调用必须以可操作诊断失败（记录函数名与参数），而不是静默跳过；
3. 复用上游的 `nvn_cover.h` 等纯函数与既有 C 单测（这些已零改动通过）；
4. 面向这个项目的目标形态重新设计命令录制与提交。

取舍：是否复用上游的 record/replay 命令模型。
(a) 复用——与上游保持一致，便于对照调试，但会继承其面向 Metal 的假设；
(b) 新建——按 Vulkan 的命令缓冲/管线模型重新设计，更简洁，但与上游对照更难。
倾向 (b) 并保留 (a) 的记录格式作为**诊断输出**，用于定位首个未实现调用。

### 四、着色器链：SPIR-V → Vulkan

1. 复用 Ryujinx 翻译器的 SPIR-V 出口，去掉 `spirv-cross` 与 metallib 环节；
2. 上游已产出 `.spv` + 反射 JSON（见 `Program.cs` 写出的 `*.spv` / `*.res.json`），
   Vulkan 侧按该反射信息建 descriptor 布局；
3. 离线出包，与上游一致——不在运行时装翻译。

`.NET` 环境（本文原视为阻塞项，现已解决）：

- `dmsh_xlate.csproj` 与 Ryujinx 依赖统一由 `Directory.Build.props` 锁定
  `TargetFramework=net10.0` / `LangVersion=preview`；
- 本机已装 `dotnet10-sdk 10.0.401`（来自 Alpine **edge** 仓库，原生 aarch64；
  v3.21 仓库最高只有 dotnet9，无法满足 `net10.0`）；
- 两处环境适配，均**不影响上游源码**：
  - `DOTNET_GCHeapHardLimit=0x10000000`——设备可用内存约 3.6GB，
    CoreCLR 默认堆预留失败会报 `HRESULT: 0x8007000E`；
  - `libcrypto3`——edge 的 dotnet10 需要 OpenSSL 3.5 符号
    `EVP_PKEY_sign_message_init`，而 v3.21 系统库为 3.3.3 不含该符号。
    处置方式为**旁路加载**（单独解开 3.5.9 的库经 `LD_LIBRARY_PATH` 供
    dotnet 使用），不改动系统库，避免牵动 adb 的依赖链。

待完成：该旁路加载的 `libssl.so.3` 尚需补齐，之后跑通一次完整的
`dmsh_xlate` 构建即视为本文第四块的环境就绪。

## 验收证据

验收形态：**真机 + 使用者自己合法导出的游戏文件**，不进 CI。

- [x] 宿主层服务模块在 Android ARM64 目标下编译，通过数不低于本次实测基线
      how:  `tools/compile_upstream_modules.sh /tmp/dm-obj-final`
      want: `compiled_ok=31 failed=0`
      where: `docs/M-compat-matrix.md`；构建输出；对应对象在 `/tmp/dm-obj-final/`

- [x] 兼容层摘要实现通过标准向量
      how:  本机运行 `/tmp/t_hashes`；Android 目标执行 `tools/build_compat_tests.sh /tmp/compat-tests`
      want: `9 checks, 0 failure(s)`；Android 产物为 `ELF64 / AArch64`
      where: `docs/M-compat-matrix.md`；`/tmp/compat-tests/t_hashes`

- [ ] Android 应用可连续呈现且无 validation error
      how:  启动 App，`logcat -s SwitchRuntime`
      want: `presented N frames`，N ≥ 1000，无 `VK_ERROR`
      where: 设备上 `Android/data/org.mrckr.switchruntime/files/diagnostics/`

- [ ] 上游模块可被加载并进入游戏
      how:  经 `d2r-android-adapter` 接入使用者自备游戏文件
      want: 启动 → 主菜单 → **创建角色并进入离线游戏**，可操作、有声音、可存读档
      where: 运行录屏 + 日志；结果记入适配器仓库

- [ ] 未实现的 NVN 调用以可操作诊断失败
      how:  运行至首个未实现调用
      want: 日志含函数名与参数，不是静默跳过或崩溃
      where: logcat

性能口径（按决策：**本阶段不设帧率门槛**）：

- 性能在本阶段是**技术路线的约束**，不是验收门槛。含义具体化为：
  设计方案时不得选择会堵死优化的结构（例如把每帧资源分配写死在热路径、
  或按 Metal 的资源堆模型假设内存充裕）；
- 需要有**可观测性**，以便后续定位瓶颈。

- [ ] 离线游戏内可采集帧率与帧时间，并记录发热/降频
      how:  进入离线游戏后采集
      want: 给出 FPS、帧时间、内存、温度的**数据**（不设合格阈值）
      where: 结果记入适配器仓库

- [ ] 图形层为后续优化留出可观测点
      how:  查看实现
      want: 存在按帧统计（提交次数、描述符更新、管线切换）的观测口，
            不要求此刻就有优化
      where: 代码 + 日志

## 数据边界检查

- 不含游戏二进制 / ROM / NSP / XCI：确认。本文只描述加载与运行时行为。
- 不含密钥：确认。未提及 `prod.keys` 或任何密钥。
- 不含从游戏提取的 shader / 资源 / 存档：确认。着色器链只描述**工具与流程**
  （Ryujinx 翻译器 → SPIR-V → Vulkan），不含任何具体 shader 内容或产物。
- RomFS 派生头文件：**存在依赖，需在实现阶段注意**。上游的 `dm_hud_exp.h`、
  `dm_vault_tab.h`、`dm_bright_levels.inc` 由**使用者自己的游戏文件**生成。
  本文只把它们记为「由本地构建步骤生成的产物」，未把内容或数据写入本文，
  实现时也不得提交。
- 其他：`dmsh_chain.py` 的设计（链版本由源码内容 sha256 决定）值得保留，
  它保证了构建产物只由源码决定，适合纳入本项目的构建规范。

## 范围外

- **D2R 适配器内容**——游戏版本指纹、补丁、地址映射、Shader 清单、输入映射、
  存档、触屏 UI。归 `d2r-android-adapter`。本文只在验收环节依赖它。
- **塞尔达（或其他第二款游戏）的迁移**——按用户指示，等本文所辖内容完成后再议。
  本文不做任何为它预留的设计。
- **`dm/host/` 下 38 个 `dm_*.c` 游戏功能模块**——归 adapter，不进 runtime。
- **iOS/tvOS 侧的任何改动**——上游 Apple 实现保持原样，作为技术参考。
- **性能优化**——性能只作为验收观测项，不设优化目标。

## 未决问题

1. **子集边界怎么定？** 「D2R 实际触发的 NVN 函数」需要运行才能枚举，
   而运行又需要前三块先完成。需要定一个**渐进式探测方法**：
   按阶段分批放行，每批跑一次并收集「未实现调用」诊断，据此扩表，
   而不是等全部实现完再发现缺哪些。这是本阶段最需要先想清楚的方法论问题。
2. **`hle_audio.c` 的 Android 实现选型**——走 AAudio 还是 OpenSL ES。
   倾向 AAudio（minSdk 29 可用，与项目 targetSdk 35 匹配）。
3. **`libssl.so.3` 旁路补齐**——环境适配的最后一步，补完即可跑通
   `dmsh_xlate` 构建。属收尾工作，无方案性风险。

已定的（原为未决，现记结论）：

- **帧率验收线**：本阶段不设。性能只作为技术路线约束（见「验收证据」）。
- **.NET 环境**：装 Alpine edge 的 `dotnet10-sdk`，两处旁路适配不改上游源码。
