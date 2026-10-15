# Switch Android Runtime

Android ARM64 runtime and compatibility framework for running **your own** legally
dumped Switch game files natively. Not an emulator: it loads the game's own
AArch64 code and provides the Switch system services, graphics API translation
and Android platform layer around it.

Status: **Phase 2 — Vulkan bring-up verified on real hardware.**

```
instance created apiVersion=1.1
device name=Adreno (TM) 730 api=1.1.128
swapchain 2520x1080, 5-6 images, FIFO/MAILBOX present modes
presented 2760 frames, ~55 fps steady
```

## What this repository is

Two layers live here, and they are deliberately separated:

| Layer | Responsibility | Must not contain |
|---|---|---|
| Runtime (this repo) | ARM64 module loading, memory/thread primitives, HLE services, NVN→Vulkan translation, input, audio, Android platform glue, diagnostics | Anything specific to one game |
| Game adapter (separate repo) | Binary offsets, patches, symbol maps, shader manifests, resource paths, save handling, touch UI | Framework logic that other games need |

The first adapter is [d2r-android-adapter](https://github.com/MrCKR/d2r-android-adapter).

## ABI policy

The framework does not restrict ABI. Interfaces and directory layout allow
adding `arm64-v8a`, `armeabi-v7a`, `x86` or `x86_64`; which ABIs a given build
contains is decided only by which native libraries that build produces. The
current test device is ARM64, so the initial phase generates `arm64-v8a` only.
That is a build artifact, not a framework limit.

## Roadmap

| Phase | Goal | State |
|---|---|---|
| 0 | Repo, architecture, data boundaries | done |
| 1 | Android app shells out to native, Vulkan instance + capability probe | done |
| 2 | Device, swapchain, render pass, clear + present loop | done |
| 3 | Shader toolchain: SPIR-V ingestion, pipeline cache, first triangle from translated code | next |
| 4 | ARM64 module loader: NSO parse, relocation, memory mapping | planned |
| 5 | HLE skeleton: threads, sync, time, filesystem, logging | planned |
| 6 | NVN API surface: objects, commands, descriptor model | planned |
| 7 | D2R adapter hookup: version ID, patches, first playable frame | planned |
| 8 | Touch input, audio, save, performance work | planned |

## Building

The build host in use is an ARM64 Linux environment, which needs one shim: the
Android SDK's AAPT2 ships as an x86_64 binary only. It is a packaging tool, so it
runs under QEMU while all target code is compiled natively.

```
tools/setup_host_arm64.sh          # once: SDK, NDK, host tools, AAPT2 shim
ANDROID_SDK_ROOT=<sdk> tools/build_arm64.sh
```

`tools/build_native_arm64.sh` compiles `app/src/main/cpp/*.cpp` with the host's
native clang, targeting `aarch64-linux-android`, and stages the result plus
`libc++_shared.so` in `app/src/main/jniLibs/`. Gradle then only does Java,
resources, packaging and signing.

On an x86_64 host none of that is needed: standard Gradle + NDK works directly.

## Diagnostics

Launching the app writes a full Vulkan capability report to
`Android/data/org.mrckr.switchruntime/files/diagnostics/vulkan_report.txt`
(limits, features, memory heaps, queue families, and the extension subset the
NVN translation layer depends on), while the same lines go to logcat under the
`SwitchRuntime` tag.

## Data boundaries

This repository contains no game code, no game assets, no shaders extracted from
a game, no encryption keys and no dumped files. It only loads files the user
supplies from their own device. The MIT license covers this project's own code
and grants no rights to any game or platform content.

## License

MIT
