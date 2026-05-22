CC ?= cc
CFLAGS ?= -Wall -Wextra -Werror -std=c11
CPPFLAGS ?= -Iinclude

SRCS := src/boot.c src/debug_uart.c src/dvfs.c src/eye_roc.c src/geometry.c src/pctl_phy_plan.c src/register_plan.c src/tables.c src/tags.c src/timing.c src/training.c
TEST_BIN := tests/rk3576_ddr_trainer_test

.PHONY: all test clean

all: $(TEST_BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(SRCS) tests/test_main.c include/rk3576_ddr/*.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRCS) tests/test_main.c -o $@

clean:
	rm -f $(TEST_BIN)
