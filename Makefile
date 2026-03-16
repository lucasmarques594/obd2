CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -O2
CFLAGS += -Wno-unused-parameter
CFLAGS += -I.

SRC_DIR = .
BUILD_DIR = build

CORE_SRCS = \
	core/ring_buffer/ring_buffer.c \
	core/str_utils/str_utils.c \
	core/error/error_handler.c \
	core/logger/logger.c \
	core/state_machine/state_machine.c \
	core/elm327/elm327.c \
	core/obd2/obd2.c \
	core/pid/pid_manager.c \
	core/dtc/dtc_manager.c \
	core/freeze_frame/freeze_frame.c \
	core/readiness/readiness.c \
	core/vehicle_info/vehicle_info.c \
	core/scheduler/scheduler.c \
	core/sanity_check/sanity_check.c

BRIDGE_SRCS = \
	ios_bridge/bluetooth_if.c

ALL_SRCS = $(CORE_SRCS) $(BRIDGE_SRCS)

OBJS = $(ALL_SRCS:%.c=$(BUILD_DIR)/%.o)

LIB_NAME = libobd2_core.a
SCANNER_BIN = obd2_scanner

.PHONY: all scanner clean dirs check info

all: dirs $(BUILD_DIR)/$(LIB_NAME)

scanner: all
	$(CC) $(CFLAGS) test_macos.c -L$(BUILD_DIR) -lobd2_core -lm -o $(SCANNER_BIN)

dirs:
	@mkdir -p $(BUILD_DIR)/core/ring_buffer
	@mkdir -p $(BUILD_DIR)/core/str_utils
	@mkdir -p $(BUILD_DIR)/core/error
	@mkdir -p $(BUILD_DIR)/core/logger
	@mkdir -p $(BUILD_DIR)/core/state_machine
	@mkdir -p $(BUILD_DIR)/core/elm327
	@mkdir -p $(BUILD_DIR)/core/obd2
	@mkdir -p $(BUILD_DIR)/core/pid
	@mkdir -p $(BUILD_DIR)/core/dtc
	@mkdir -p $(BUILD_DIR)/core/freeze_frame
	@mkdir -p $(BUILD_DIR)/core/readiness
	@mkdir -p $(BUILD_DIR)/core/vehicle_info
	@mkdir -p $(BUILD_DIR)/core/scheduler
	@mkdir -p $(BUILD_DIR)/core/sanity_check
	@mkdir -p $(BUILD_DIR)/ios_bridge

$(BUILD_DIR)/$(LIB_NAME): $(OBJS)
	ar rcs $@ $^

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(SCANNER_BIN)

check: $(ALL_SRCS)
	@echo "Checking syntax..."
	@for src in $(ALL_SRCS); do \
		$(CC) $(CFLAGS) -fsyntax-only $$src && echo "OK: $$src"; \
	done

info:
	@echo "Source files:"
	@for src in $(ALL_SRCS); do echo "  $$src"; done
	@echo ""
	@echo "Object files:"
	@for obj in $(OBJS); do echo "  $$obj"; done