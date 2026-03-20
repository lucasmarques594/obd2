CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -O2
CFLAGS += -Wno-unused-parameter
CFLAGS += -I.

CFLAGS_LLM = -Wall -Wextra -Werror -std=c99 -O2
CFLAGS_LLM += -Wno-unused-parameter
CFLAGS_LLM += -I.

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

UNITY_SRC = test/unity/unity.c

TEST_SRCS = \
	test/test_ring_buffer.c \
	test/test_str_utils.c \
	test/test_pid_manager.c \
	test/test_dtc_manager.c \
	test/test_state_machine.c \
	test/test_sanity_check.c \
	test/test_error_handler.c \
	test/test_llm_dtc.c

TEST_BINS = $(TEST_SRCS:test/%.c=$(BUILD_DIR)/test/%)

.PHONY: all scanner clean dirs check info test

all: dirs $(BUILD_DIR)/$(LIB_NAME)

scanner: all
	$(CC) $(CFLAGS_LLM) test_macos.c core/llm/llm_dtc.c -L$(BUILD_DIR) -lobd2_core -lcurl -lm -o $(SCANNER_BIN)

scanner-ble: all
	clang -std=c99 -Wall -Wextra -O2 -Wno-unused-parameter -I. \
		-fobjc-arc \
		test_macos_ble.m ble_bridge/ble_elm327.m core/llm/llm_dtc.c \
		-L$(BUILD_DIR) -lobd2_core \
		-framework Foundation -framework CoreBluetooth -lcurl -lm \
		-o obd2_scanner_ble

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
	rm -rf $(BUILD_DIR) $(SCANNER_BIN) obd2_scanner_ble

test: all $(TEST_BINS)
	@echo ""
	@echo "=== Running tests ==="
	@echo ""
	@failed=0; \
	for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		./$$t || failed=1; \
		echo ""; \
	done; \
	if [ $$failed -eq 0 ]; then \
		echo "=== ALL TESTS PASSED ==="; \
	else \
		echo "=== SOME TESTS FAILED ==="; \
		exit 1; \
	fi

$(BUILD_DIR)/test/test_llm_dtc: test/test_llm_dtc.c core/llm/llm_dtc.c $(BUILD_DIR)/$(LIB_NAME)
	@mkdir -p $(BUILD_DIR)/test
	$(CC) $(CFLAGS_LLM) -Wno-unused-function -Itest test/test_llm_dtc.c core/llm/llm_dtc.c $(UNITY_SRC) -L$(BUILD_DIR) -lobd2_core -lcurl -lm -o $@

$(BUILD_DIR)/test/%: test/%.c $(BUILD_DIR)/$(LIB_NAME)
	@mkdir -p $(BUILD_DIR)/test
	$(CC) $(CFLAGS) -Wno-unused-function -Itest $< $(UNITY_SRC) -L$(BUILD_DIR) -lobd2_core -lm -o $@

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
