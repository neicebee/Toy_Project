CC = clang
CFLAGS = -Wall -Wextra -std=c11 -g
LDFLAGS = -framework CoreFoundation -framework Security

# 목표 바이너리
BUILD_DIR = ../../build
TARGET = $(BUILD_DIR)/dylib_auditor

# 소스 파일들
SOURCES = main.c \
          binary_scanner.c \
          macho_parser.c \
          code_signing.c \
          vuln_scanner.c \
          hijack_detector.c \
          process_report.c \
          process_scanner.c \
          path_utils.c \
          options.c

OBJECTS = $(SOURCES:.c=.o)

# 기본 규칙
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	@echo "[링킹] $(OBJECTS) → $(TARGET)"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJECTS) && echo "✓ 빌드 완료: $(TARGET)" || echo "[ERROR] 링킹 실패"

%.o: %.c
	@echo "[컴파일] $<"
	@$(CC) $(CFLAGS) -c $< && echo "  ✓" || (echo "  [ERROR]" && exit 1)

clean:
	@echo "[정리] 빌드 산출물 제거"
	@rm -f $(OBJECTS) $(TARGET) *.o

.PHONY: all clean
