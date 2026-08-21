/**
 * Level1Framework.c
 * 
 * Type C 검증용 Level 1 Framework (최상위)
 * ModularApp의 직접 의존성
 * Level2Framework에 의존
 */

#include <stdio.h>
#include <stdlib.h>
#include "ModularFramework.h"

/* Level2Framework 함수 선언 */
extern void level2_init(void);
extern void level2_process(const char *data);
extern const char* level2_version(void);

/* ================================================================
 * Level 1 Framework 구현
 * ================================================================ */

void level1_init(void) {
    printf("[Level1Framework] Initializing...\n");
    printf("[Level1Framework] Loading Level2Framework...\n");
    level2_init();
}

void level1_process(const char *data) {
    if (!data) return;
    printf("[Level1Framework] Processing: %s\n", data);
    printf("[Level1Framework] Delegating to Level2Framework...\n");
    level2_process(data);
}

const char* level1_version(void) {
    return "1.0.0";
}

__attribute__((constructor))
void level1_constructor(void) {
    printf("[Level1Framework:Constructor] Entry point - Level 1 loaded\n");
    printf("[Level1Framework:Constructor] Version: %s\n", level1_version());
    printf("[Level1Framework:Constructor] *** 이 시점이 최고 위험! ***\n");
    printf("[Level1Framework:Constructor] *** Type C 공격의 최적 시점 ***\n");
    level1_init();
}

__attribute__((destructor))
void level1_destructor(void) {
    printf("[Level1Framework:Destructor] Cleanup\n");
}
