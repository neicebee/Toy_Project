/**
 * test_app_type_c.c
 * 
 * Type C (모듈화 Framework 체인) 주입 검증용 예제 앱
 * 3단계 Framework 의존성 체인을 가진 앱
 * 
 * 의존성:
 * ModularApp → Level1Framework → Level2Framework → Level3Framework
 * 
 * 컴파일:
 * gcc -o ModularApp test_app_type_c.c \
 *     -F./Frameworks/Level1Framework.framework/Versions/A \
 *     -framework Level1Framework \
 *     -rpath @loader_path/Frameworks
 * 
 * 디렉토리 구조:
 * ModularApp/
 * ├── ModularApp (실행 파일)
 * └── Frameworks/
 *     ├── Level1Framework.framework/
 *     │   └── Versions/A/
 *     │       └── Level1Framework (바이너리)
 *     ├── Level2Framework.framework/
 *     │   └── Versions/A/
 *     │       └── Level2Framework (바이너리)
 *     └── Level3Framework.framework/
 *         └── Versions/A/
 *             └── Level3Framework (바이너리)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Level1Framework 함수 선언 */
extern void level1_init(void);
extern void level1_process(const char *data);
extern const char* level1_version(void);

int main(int argc, char *argv[]) {
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║         ModularApp - Type C Framework 체인 검증       ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    printf("[*] 앱 시작\n");
    printf("[*] 3단계 Framework 의존성 체인 사용:\n");
    printf("    ModularApp\n");
    printf("      ↓\n");
    printf("    Level1Framework (최고 위험!)\n");
    printf("      ↓\n");
    printf("    Level2Framework\n");
    printf("      ↓\n");
    printf("    Level3Framework\n\n");
    
    printf("[*] Framework 함수 호출:\n");
    printf("    - level1_version(): %s\n", level1_version());
    
    printf("\n[*] Framework 처리 시작:\n");
    printf("    [경로] ModularApp → Level1 → Level2 → Level3\n\n");
    level1_process("Data from ModularApp");
    
    printf("\n[✓] 모든 Framework 로드 및 처리 완료\n");
    printf("[*] 앱 종료\n");
    printf("[*] Framework 클린업 완료\n\n");
    
    return 0;
}
