BUILD_DIR := build

.PHONY: all config build run test clean

all: build run

config:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

build:
	cmake --build $(BUILD_DIR)

run:
	./$(BUILD_DIR)/microsql

TEST_NAMES := $(patsubst tests/%.cpp,%,$(wildcard tests/*.cpp))
TEST_BINS  := $(addprefix $(BUILD_DIR)/,$(TEST_NAMES))
TEST_TARGETS := $(addprefix --target ,$(TEST_NAMES))

test:
	cmake --build $(BUILD_DIR) $(TEST_TARGETS)
	$(foreach bin,$(TEST_BINS),./$(bin);)

clean:
	rm -rf $(BUILD_DIR)
