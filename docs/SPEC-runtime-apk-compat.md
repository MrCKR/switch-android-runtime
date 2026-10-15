> 状态：实施中（用户已批准）。本 spec 与代码在同一工作阶段推进。

# Spec: runtime APK 集成兼容层自检

## 目标与范围

对应基础框架移植工作的第一个垂直切片：把已在命令行 Android ARM64 目标下验证的
Darwin 兼容层编译进 `switch-android-runtime` APK，并在真机启动时执行最小自检。

完成后必须能够证明：

```text
APK 启动 → native runtime 初始化 → compat self-test → Vulkan 现有呈现链继续运行
```

本 spec 不实现 NSO/ELF loader、不实现 NVN→Vulkan、不接入 D2R 游戏文件，也不改变
D2R adapter 仓库。

## 归属

主导仓库：`switch-android-runtime`。

这些能力不需要知道“这是 D2R”，属于 runtime：Native 构建集成、兼容层自检、日志和
诊断文件。D2R 只在后续模块加载验收时作为真实测试对象出现。

## 现状与依据

- 现有 APK 的 native 入口是 `runtime.cpp`，Android Surface 与 Vulkan clear/present
  已在 vivo V2256A 上跑通，证据见 `docs/PHASE2.md`。
- 兼容层目前以 C 源文件和 Darwin 仿制头存在，宿主层编译矩阵为
  `compiled_ok=31 failed=0`，证据见 `docs/M-compat-matrix.md`。
- SHA256/MD5 标准向量在 AArch64 Android 真机上为 `9 checks, 0 failure(s)`。
- 当前 `app/src/main/cpp/CMakeLists.txt` 只编译 `runtime.cpp`，所以上述兼容层还没有
  进入 APK；这就是本 spec 要解决的具体缺口。

## 方案

1. 将兼容层的实现源加入 Android native target：摘要、Blocks、fcntl、xattr、Mach-O
   注册、AAudio AudioToolbox 适配；保持上游源码不修改。
2. 在 runtime 内增加一个小型 C 自检入口，验证：
   - SHA256 空输入、`abc` 和分块输入；
   - MD5 回归向量；
   - `pthread_compat` 的当前线程栈信息与单调时钟可调用；
   - 自检失败返回非零，并写入 Android log 与诊断报告。
3. `CMakeLists.txt` 统一设置兼容头 include 路径和 `-fblocks`，链接 `aaudio`；不复制
   一份不同的兼容实现到 C++ 入口。
4. `runtime.cpp` 在已有报告打开后调用自检入口；结果只写日志/报告，不阻断现有 Vulkan
   probe。自检失败必须可见，不得静默继续。
5. 保持构建 ABI 可扩展；当前设备产物仍按现有构建流程生成 `arm64-v8a`，不在 CMake
   中加入 ABI 限制。

## 验收证据

- [x] APK native target 包含兼容层并成功链接
      how: `tools/build_arm64.sh`
      want: Gradle assemble 成功；APK 内含 `libswitch_runtime.so` 与 `libc++_shared.so`
      where: 构建输出；APK 解包结果（`lib/arm64-v8a/`）

- [x] 真机启动执行 compat self-test
      how: 安装 APK，启动后读取 `diagnostics/vulkan_report.txt`
      want: `compat self-test: PASS`，且无 `compat self-test: FAIL`
      where: `Android/data/org.mrckr.switchruntime/files/diagnostics/vulkan_report.txt`

- [x] 兼容层接入后 Vulkan 呈现链不回归
      how: 在同一 vivo V2256A 上启动并保持前台运行
      want: `device name=Adreno (TM) 730`、`swapchain created: 2520x1080 images=6`、`presented 3600 frames, 54.4 fps`
      where: 设备 diagnostics 报告；无 `FATAL`/`SIGSEGV` 启动崩溃

## 数据边界检查

- 不包含游戏二进制、ROM、NSP/XCI：确认。
- 不包含密钥、提取的 shader、贴图、音频、文本或存档：确认。
- 自检只使用固定标准摘要向量和运行时自身状态：确认。

## 范围外

- NSO/ELF 模块加载和重定位；
- NVN→Vulkan 对象、命令和 shader pipeline；
- D2R 版本识别、补丁、地址、资源挂载；
- 进入主菜单或离线游戏；
- 塞尔达适配；
- FPS 合格线和性能优化。

## 完成后续

本 slice 完成后，下一项是合成 ARM64 模块的 loader 测试，再把 loader 接进 APK；
仍然不直接加载真实 D2R，直到合成模块通过确定性入口/重定位验收。

## APK integration evidence

The first APK build exposed an integration error: the native build script still
linked only `runtime.cpp`, so the APK had an unresolved `dm_compat_self_test`
symbol at `System.loadLibrary`. The script was then changed to compile every
`runtime/compat/dm_compat_*.c` object as Android C and link those objects into
`libswitch_runtime.so`.

Final build:

```text
BUILD SUCCESSFUL
APK: app/build/outputs/apk/debug/app-debug.apk
APK contains:
  lib/arm64-v8a/libswitch_runtime.so
  lib/arm64-v8a/libc++_shared.so
```

Final device report:

```text
compat self-test: PASS
instance created apiVersion=1.1
device name=Adreno (TM) 730 ... api=1.1.128
swapchain created: 2520x1080 images=6 presentMode=1
presented 3600 frames, 54.4 fps (one clear-only frame in flight)
```

No `FATAL`, `SIGSEGV`, or `compat self-test: FAIL` occurred in the final
launch. The report was read from the app's external-files diagnostics path:

```text
/sdcard/Android/data/org.mrckr.switchruntime/files/diagnostics/vulkan_report.txt
```
