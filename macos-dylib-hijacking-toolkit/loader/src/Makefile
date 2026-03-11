CC = clang
CFLAGS = -Wall -Wextra -g
LDFLAGS = 

# 목표 바이너리
BUILD_DIR = ../../build
TARGET = $(BUILD_DIR)/dylib_loader

# 소스 파일들
SOURCES = main.c result_parser.c rpath_injector.c weak_dylib_injector.c dylib_modifier.c
OBJECTS = $(SOURCES:.c=.o)

# 기본 규칙
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	@echo "[링킹] $(OBJECTS) → $(TARGET)"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJECTS) && echo "✓ 빌드 완료: $(TARGET)" || echo "[ERROR] 링킹 실패"

%.o: %.c
	@echo "[컴파일] $<"
	@$(CC) $(CFLAGS) -c $<

clean:
	@echo "[정리] 빌드 산출물 제거"
	@rm -f $(OBJECTS) $(TARGET)
	@echo "✓ 정리 완료"

.PHONY: all clean
