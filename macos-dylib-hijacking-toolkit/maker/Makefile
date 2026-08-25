CC = clang
CFLAGS = -Wall -Wextra -O2
TARGET = type_a_verify

.PHONY: all build run clean

all: build

build: $(TARGET)
	@echo "✓ Build complete: ./$(TARGET)"

$(TARGET): type_a_verify.c
	$(CC) $(CFLAGS) -o $(TARGET) type_a_verify.c
	@chmod +x $(TARGET)

run: build
	@echo "╔════════════════════════════════════════════════════════╗"
	@echo "║         Type A Verification - IINA.app Test            ║"
	@echo "╚════════════════════════════════════════════════════════╝"
	@./$(TARGET)

clean:
	rm -f $(TARGET) malicious_libsoxr.c malicious_libsoxr.dylib
	@echo "✓ Cleanup complete"

help:
	@echo "Available targets:"
	@echo "  make build  - Compile type_a_verify binary"
	@echo "  make run    - Compile and run verification"
	@echo "  make clean  - Remove generated files"
	@echo "  make help   - Show this help message"
