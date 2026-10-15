# Switch Android Runtime

面向 Android ARM64 的 Switch 游戏原生兼容运行时与游戏适配框架。

> 项目定位：为特定 Switch 游戏提供可复用的 Android 运行时能力，再由独立的 Game Adapter 实现具体游戏。
>
> 当前状态：**架构验证阶段，尚未实现可运行的 Android 构建**。

## 目标

本项目不是从第一天就承诺成为完整的通用 Switch 模拟器，而是建立一个可扩展的运行时：

```text
Android App
    └── Platform Layer
        └── Runtime Core
            ├── ARM64 module loader
            ├── Switch service/HLE layer
            ├── virtual filesystem
            ├── thread/synchronization/time
            ├── input/audio
            └── graphics API layer
                └── Vulkan backend
                    └── Game Adapter
```

第一项验证目标是：配合 [`d2r-android-adapter`](https://github.com/MrCKR/d2r-android-adapter)，在真实 Android 手机上启动并运行 Diablo II: Resurrected 的 Switch 版本。

## 与传统模拟器的区别

- 不以模拟完整 Switch 硬件为第一目标；
- 优先利用 Switch 与 Android 手机共有的 ARM64 指令集，直接运行目标游戏代码；
- 只实现目标游戏实际使用的 Switch 服务；
- 将 Switch NVN 图形调用映射到 Android Vulkan；
- 具体游戏的地址、补丁、Shader 清单和特殊行为放在 Adapter 仓库，而不是写死在核心运行时。

这是一个**游戏专用兼容层框架**，不是现成的 Switch 通用模拟器。

## 仓库边界

### 本仓库负责

- Android NDK/CMake 工程和 APK 外壳；
- ARM64 模块加载、映射、重定位和生命周期；
- 通用线程、同步、时间、内存和异常基础设施；
- Switch 文件系统和服务兼容接口；
- NVN 抽象层及 Vulkan 后端；
- Shader 转换、反射和 Vulkan pipeline cache；
- Android 输入、触屏、手柄、音频和文件导入；
- 日志、诊断、设备能力探测和测试工具；
- 稳定的 Game Adapter SDK/API。

### Game Adapter 负责

- 支持的游戏版本与指纹；
- 游戏模块地址、符号和重定位信息；
- 游戏专用二进制补丁；
- 特殊系统服务覆盖；
- Shader/纹理/资源清单；
- 存档、输入映射和游戏专用 UI；
- 版本兼容性与已知问题。

## 计划

### Phase 0：架构和设备基线

- [x] 建立独立 Runtime 和 D2R Adapter 仓库；
- [ ] 固定 Adapter SDK 边界；
- [ ] 记录目标手机的 SoC、GPU、Vulkan 和内存能力；
- [ ] 建立最小 CI：格式检查、CMake 配置、单元测试。

### Phase 1：Android 最小运行骨架

- [ ] Gradle + Android NDK + CMake 工程；
- [ ] NativeActivity/Activity + Surface 生命周期；
- [ ] Vulkan instance/device/swapchain；
- [ ] 黑屏清屏 Demo 和 GPU 能力报告；
- [ ] Android 日志、崩溃转储和可导出的诊断包。

### Phase 2：通用 Runtime

- [ ] ARM64 模块格式解析；
- [ ] 模块加载、映射和重定位；
- [ ] 内存区域模型；
- [ ] 线程、TLS、锁、事件和计时器；
- [ ] 虚拟文件系统和只读游戏资源挂载；
- [ ] 最小 Switch 服务/HLE 接口。

### Phase 3：图形和媒体

- [ ] NVN 对象和命令抽象；
- [ ] Buffer、Texture、Sampler、Framebuffer；
- [ ] Shader → SPIR-V/Vulkan pipeline 流程；
- [ ] 同步、呈现和 pipeline cache；
- [ ] 音频输出和手柄输入。

### Phase 4：D2R 验证

- [ ] 接入 `d2r-android-adapter`；
- [ ] 游戏文件指纹校验和用户导入流程；
- [ ] 启动器和资源目录映射；
- [ ] 启动到主菜单；
- [ ] 创建角色并进入离线游戏；
- [ ] 输入、声音、存档和恢复；
- [ ] 在目标 vivo 手机上持续运行和性能记录。

### Phase 5：抽象验证

- [ ] 将 D2R 专用逻辑全部移出 Runtime；
- [ ] 为第二个实验游戏建立最小 Adapter；
- [ ] 记录哪些接口真正通用，哪些必须保留为游戏扩展；
- [ ] 冻结 v0.1 Adapter SDK。

## 目标设备

第一验证设备来自当前开发环境：

- vivo V2256A / PD2256；
- Android 16，SDK 36；
- ARM64（`arm64-v8a`）；
- Qualcomm `taro` 平台；
- 具体 GPU/Vulkan 能力需要由 Phase 1 工具实际读取，不能仅凭 SoC 名称推断。

## 合法性和数据边界

本项目不分发 Nintendo、Blizzard 或其他第三方的游戏程序、资源、密钥、Shader、存档或受版权保护的数据。用户必须自行从合法拥有的游戏副本导出文件。

任何提交都不得包含：

- 游戏 ROM/NSP/XCI；
- `prod.keys` 或其他密钥；
- 游戏二进制、资源和存档；
- API Token、签名证书和私钥；
- 从游戏内容直接提取的可再分发文件。

## 许可证

框架代码暂采用 MIT License。具体第三方依赖遵循各自许可证。

## 相关项目

- [D2R Android Adapter](https://github.com/MrCKR/d2r-android-adapter)
- [上游 d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile)
