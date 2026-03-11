.PHONY: all build clean scanner loader payload help

all: build

build: scanner loader payload

scanner:
	@echo "┌─────────────────────────────┐"
	@echo "│  Building Scanner Module... │"
	@echo "└─────────────────────────────┘"
	@$(MAKE) -C scanner/src all

loader:
	@echo "┌─────────────────────────────┐"
	@echo "│  Building Loader Module...  │"
	@echo "└─────────────────────────────┘"
	@$(MAKE) -C loader/src all

payload:
	@echo "┌─────────────────────────────┐"
	@echo "│  Payload Template Ready     │"
	@echo "└─────────────────────────────┘"
	@echo "  ℹ️  컴파일은 수동으로 수행하세요 (payload/README.md 참고)"

clean:
	@echo "┌─────────────────────────────┐"
	@echo "│  Cleaning Build Artifacts   │"
	@echo "└─────────────────────────────┘"
	@$(MAKE) -C scanner/src clean
	@$(MAKE) -C loader/src clean
	@rm -rf build/*
	@echo "✓ 정리 완료"

help:
	@echo "macOS Dylib Hijacking Toolkit"
	@echo ""
	@echo "Available targets:"
	@echo "  make build        - Build all modules (scanner, loader)"
	@echo "  make scanner      - Build scanner module only"
	@echo "  make loader       - Build loader module only"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make all          - Same as 'make build'"
	@echo "  make help         - Show this help message"
