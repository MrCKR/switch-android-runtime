# Architecture Plan

## Guiding rule

The runtime must not know D2R concepts. D2R-specific behavior is exposed through an adapter interface and loaded by an explicit manifest.

## Initial modules

```text
runtime/
  loader/       ARM64 modules, mappings, relocations
  memory/       virtual memory model and permissions
  kernel/       threads, TLS, locks, events, clocks
  hle/          Switch service interfaces and dispatch
  fs/           virtual filesystem and resource mounts
  graphics/     NVN-neutral objects and command model
  vulkan/       Android Vulkan backend
  audio/        Android audio output
  input/        touch, gamepad, virtual controls
  diagnostics/  logging, traces, crash reports
android/
  app/          Activity, lifecycle, Surface
  jni/          Java/Kotlin ↔ native bridge
  packaging/    Gradle/NDK packaging
sdk/
  adapter_api/  stable adapter interfaces
  manifest/     version and capability schema
tools/
  inspect/      module and runtime inspection
  shader/       offline shader/pipeline tools
  test/         host-side conformance tests
```

## Adapter contract

An adapter should provide:

- identity and supported version fingerprints;
- module layout and loader policy;
- patch and relocation descriptions;
- service overrides;
- graphics profile and shader manifest;
- resource mount rules;
- input and display profile;
- save-game policy;
- compatibility tests.

## Non-goals for v0.1

- full Switch hardware emulation;
- support for every Switch game;
- online service emulation;
- distribution of copyrighted game data;
- automatic downloading of game files;
- support for 32-bit ARM or x86 in the first milestone.

## First technical proof

The first meaningful proof is not a blank Vulkan screen. It is a small host-side test that loads an adapter manifest, validates a user-provided game directory without exposing its contents, maps a test ARM64 module, and reports capabilities. The Android milestone then adds rendering and lifecycle.
