# RK3576 DDR Trainer

RK3576 DDR Trainer is an open source C module for DDR calibration and handoff on Rockchip RK3576 systems. It provides host-buildable calibration logic for LPDDR4, LPDDR4X, and LPDDR5 configurations, with deterministic builders for timing data, training plans, DVFS records, memory-region tags, UART setup, and handoff gate programming.

The code is structured as a small embedded C library. Most modules are pure builders that produce records or register-step plans, so board ports can review or replay the generated operations before connecting them to platform MMIO routines.

## What is included

- DDR timing descriptor and derived timing builders for LPDDR4, LPDDR4X, and LPDDR5
- PHY frequency, WCK, PCTL patch, and DVFS table support
- DDR config tag and boot export image builders
- Runtime geometry packing and chip-select size calculation
- DVFS FSP record construction and channel snapshot helpers
- Training, PCTL, PHY, rate-change, UART, OTP gate, handoff, and eye-scan plan helpers
- A host-side test program that builds with a normal C compiler

## Layout

```text
include/rk3576_ddr/  Public headers
src/                 Implementation files
tests/               Host test program
Makefile             Simple build and test entry points
```

## Requirements

You need a C11 compiler and `make` for the host tests. On macOS or Linux, the system `cc` is enough.

To build a raw RK3576 image, install LLVM with AArch64 bare-metal support. The default binary build uses `clang`, `lld`, and `llvm-objcopy`, and you can override those tools through `CROSS_CC` and `LLVM_OBJCOPY`.

## Build

```sh
make
```

This builds the host test binary at:

```text
tests/rk3576_ddr_trainer_test
```

## Build a raw RK3576 image

```sh
make bin
```

This produces:

```text
build/rk3576_ddr_trainer.elf
build/rk3576_ddr_trainer.bin
build/rk3576_ddr_trainer.map
```

The image entry point is `_start` at `0x3ff81000`. Startup code sets the stack, clears BSS, preserves the incoming boot-parameter pointer in `x0`, and calls `rk3576_ddr_entry()`.

If LLVM is not first in your shell path, pass the tools explicitly:

```sh
make bin \
  CROSS_CC=/opt/homebrew/opt/llvm/bin/clang \
  LLVM_OBJCOPY=/opt/homebrew/opt/llvm/bin/llvm-objcopy
```

Board defaults live in `target/rk3576_board.c`. Adjust that file for the DRAM population, channel map, MR8 values, address-map policy, debug UART clock, and next-stage handoff address used by your board.

## Test

```sh
make test
```

The test binary exercises the public builders and plan serializers, including timing selection, DDR config tags, geometry packing, DVFS record generation, debug UART formatting, boot gate decisions, and eye/ROC helpers.

## Clean

```sh
make clean
```

## Using the module

Include the headers from `include/rk3576_ddr` and compile the `src/*.c` files into your boot-stage project. Pure builder APIs can be used directly. Code that needs hardware access should connect generated register steps to the board's MMIO write, read, poll, delay, and UART routines.

For the included raw image target, the platform glue is under `target/`: `startup.S` provides the boot entry, `rk3576_ddr.ld` controls SRAM placement, `rk3576_mmio.c` applies register operations, and `rk3576_entry.c` wires the board configuration into the library.

A typical integration flow is:

1. Build timing descriptors for the selected DRAM type and target rate.
2. Generate PCTL, PHY, training, and DVFS plan records.
3. Build DDR config tags and boot export data for handoff.
4. Apply register steps through the platform MMIO layer.
5. Transfer control to the next boot stage with the generated handoff data.

## Status

This project is intended for bring-up, bootloader integration, and board-specific calibration work on RK3576-based systems. Hardware ports should validate the generated register traffic on the target board and tune board policy around the provided calibration primitives.
