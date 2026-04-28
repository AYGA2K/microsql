BUILD_DIR := build

.PHONY: all config build run clean

all: build run

config:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

build:
	cmake --build $(BUILD_DIR)

run:
	./$(BUILD_DIR)/microsql

clean:
	rm -rf $(BUILD_DIR)
