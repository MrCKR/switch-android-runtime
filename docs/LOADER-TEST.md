# Synthetic loader test evidence

The synthetic loader test was built by `tools/build_loader_test.sh` and run on
the vivo V2256A. The test image is generated at runtime into `/tmp`; it is not
stored in this repository.

```text
Class: ELF64
Machine: AArch64
loader test: PASS
modules=1 relative=0x731e02b000 absolute=0x731e02b004 imported=0x123456789abcdef0 entry=42
[dm] synthetic: reloc relative=1 abs/glob=2 jump=0
rc=0
```

The loader source remains upstream code. The test-only source transformation
removes only the Darwin-private `tpidrro_el0` TSD-layout probe. That probe
segfaulted on Bionic during the first real-device run. It is not equivalent to
a completed Android TLS implementation and is tracked as the next runtime task.
