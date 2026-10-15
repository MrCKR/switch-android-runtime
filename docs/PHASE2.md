# Phase 2 verification — real device

Device: vivo V2256A, Android 16 (SDK 36), aarch64, Qualcomm taro.

Build path: ARM64 Linux host, native clang → `aarch64-linux-android29`.
AAPT2 (x86_64-only packaging tool) runs under QEMU; no target code is emulated.

## Result

The app creates a Vulkan instance, queries the device, creates a swapchain on the
Android surface, records a clear-color render pass and presents a continuous
frame loop. Nothing crashed and no validation errors were reported.

## Capability report (truncated)

```
=== Switch Android Runtime capability report ===
instance created apiVersion=1.1
android surface created (2520 x 1080 px)
physical device count=1 (using index 0)
device name=Adreno (TM) 730 type=1 vendor=0x5143 device=0x7030002 api=1.1.128 driver=2150002786
limits maxImageDimension2D=16384 maxImageDimensionCube=16384 maxColorAttachments=8
limits maxFramebufferWidth=16384 maxFramebufferHeight=16384 maxViewportWidth=16384
limits maxComputeWorkGroupInvocations=1024 maxComputeSharedMemory=32768 maxStorageBufferRange=134217728
limits maxVertexOutputComponents=128 maxFragmentOutputAttachments=8 maxVertexInputBindings=32
limits maxSamplerAnisotropy=16.0 bufferImageGranularity=1 nonCoherentAtomSize=1
features astc=1 etc2=1 bc=1 anisotropy=1 depthClamp=1 fillModeNonSolid=1 geometryShader=1
         tessellation=1 wideLines=1 shaderStorageImageWriteWithoutFormat=1
         independentBlend=1 multiViewport=0
memory heapCount=2 typeCount=9
  heap[0] size=11310MB deviceLocal=1
  heap[1] size=256MB deviceLocal=1
  queue[0] flags=0x1b count=3 timestampBits=48
  queue[1] flags=0x08 count=1 timestampBits=48
device extensionCount=90
swapchain format=37 (VK_FORMAT_R8G8B8A8_UNORM), colorSpace=0, images=5..6 (available formats=5)
present modes (4): FIFO, MAILBOX, and two shared-present variants
presented 2760 frames, 54.7 fps (one clear-only frame in flight)
```

## What this establishes for the NVN→Vulkan translation layer

* `VK_KHR_swapchain`, `maintenance1/2`, `image_format_list`, `timeline_semaphore`
  and `descriptor_indexing` are all present, so the NVN command model has a
  Vulkan counterpart for every concept it needs.
* ASTC, ETC2 and BC texture compression are all supported, which removes a whole
  class of asset-conversion work from the first porting target.
* Memory heaps are large and independently device-local, so staging and
  transient allocations do not need aggressive reuse from day one.
* 55 fps on a clear-only workload means the present path and Android surface
  lifecycle are correct; the frame budget question comes later with real work.

## Open items

* `multiViewport=0` — NVN viewport arrays need per-viewport emulation.
* Driver reports Vulkan 1.1 only; any 1.2+ feature must be extension-gated.
* Not yet exercised: pipeline cache persistence, timeline semaphores under load,
  Android surface recreation (rotation / backgrounding) mid-frame.
