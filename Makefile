CC ?= cc
CFLAGS ?= -Wall -Wextra -Werror -std=c11
CPPFLAGS ?= -Iinclude

CROSS_CC ?= clang
LLVM_OBJCOPY ?= llvm-objcopy
TARGET_CFLAGS ?= --target=aarch64-none-elf -march=armv8-a -Wall -Wextra -Werror -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -fdata-sections -ffunction-sections
TARGET_CPPFLAGS ?= -Iinclude -Itarget
TARGET_LDFLAGS ?= --target=aarch64-none-elf -fuse-ld=lld -nostdlib -Wl,--gc-sections -Wl,-T,target/rk3576_ddr.ld -Wl,-Map,build/rk3576_ddr_trainer.map

SRCS := src/boot.c src/debug_uart.c src/dvfs.c src/eye_roc.c src/geometry.c src/pctl_phy_plan.c src/register_plan.c src/tables.c src/tags.c src/timing.c src/training.c
TARGET_SRCS := $(SRCS) target/rk3576_board.c target/rk3576_entry.c target/rk3576_mmio.c target/rk3576_payload.c target/string.c
TARGET_ASMS := target/startup.S
TEST_BIN := tests/rk3576_ddr_trainer_test
TARGET_ELF := build/rk3576_ddr_trainer.elf
TARGET_BIN := build/rk3576_ddr_trainer.bin

.PHONY: all test bin clean

all: $(TEST_BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

bin: $(TARGET_BIN)

$(TEST_BIN): $(SRCS) tests/test_main.c include/rk3576_ddr/*.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRCS) tests/test_main.c -o $@

build:
	mkdir -p build

$(TARGET_ELF): $(TARGET_SRCS) $(TARGET_ASMS) target/rk3576_ddr.ld target/rk3576_board.h target/rk3576_mmio.h target/rk3576_payload.h include/rk3576_ddr/*.h | build
	$(CROSS_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) $(TARGET_SRCS) $(TARGET_ASMS) $(TARGET_LDFLAGS) -o $@

$(TARGET_BIN): $(TARGET_ELF)
	$(LLVM_OBJCOPY) -O binary $< $@

clean:
	rm -f $(TEST_BIN)
	rm -rf build
