BUILD_DIR := build

.PHONY: all config build run test clean

all: build run

config:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

build:
	cmake --build $(BUILD_DIR)

run:
	./$(BUILD_DIR)/microsql

test:
	cmake --build $(BUILD_DIR) --target lexer_test --target parser_test
	./$(BUILD_DIR)/lexer_test
	./$(BUILD_DIR)/parser_test

clean:
	rm -rf $(BUILD_DIR)
