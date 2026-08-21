/**
 * test_app_type_b.c
 * 
 * Type B (Framework 공급망) 주입 검증용 예제 앱
 * SimplePayload.framework를 사용하는 간단한 앱
 * 
 * 컴파일:
 * gcc -o TestApp test_app_type_b.c \
 *     -F./SimplePayload.framework/Versions/A \
 *     -framework SimplePayload \
 *     -rpath @loader_path/Frameworks
 * 
 * 디렉토리 구조:
 * TestApp/
 * ├── TestApp (실행 파일)
 * └── Frameworks/
 *     └── SimplePayload.framework/
 *         └── Versions/A/
 *             └── SimplePayload (바이너리)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Framework 함수 선언 */
extern void simple_payload_init(void);
extern void simple_payload_process(const char *data);
extern const char* simple_payload_version(void);

int main(int argc, char *argv[]) {
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║           TestApp - Type B Framework 검증             ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    printf("[*] 앱 시작\n");
    printf("[*] SimplePayload.framework 사용\n\n");
    
    /* Framework 함수 호출 */
    printf("[*] Framework 함수 호출:\n");
    printf("    - simple_payload_version(): %s\n", simple_payload_version());
    
    printf("\n[*] Framework 처리 시작:\n");
    simple_payload_process("Hello from TestApp");
    
    printf("\n[*] 앱 종료\n");
    printf("[*] Framework 클린업 완료\n\n");
    
    return 0;
}
