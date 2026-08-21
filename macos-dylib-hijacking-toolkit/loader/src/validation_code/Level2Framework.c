/**
 * Level2Framework.c
 * 
 * Type C 검증용 Level 2 Framework (중간)
 * Level1Framework에 의존
 * Level3Framework에 의존
 */

#include <stdio.h>
#include <stdlib.h>
#include "ModularFramework.h"

/* Level3Framework 함수 선언 */
extern void level3_init(void);
extern void level3_process(const char *data);
extern const char* level3_version(void);

/* ================================================================
 * Level 2 Framework 구현
 * ================================================================ */

void level2_init(void) {
    printf("[Level2Framework] Initializing...\n");
    printf("[Level2Framework] Loading Level3Framework...\n");
    level3_init();
}

void level2_process(const char *data) {
    if (!data) return;
    printf("[Level2Framework] Processing: %s\n", data);
    printf("[Level2Framework] Delegating to Level3Framework...\n");
    level3_process(data);
}

const char* level2_version(void) {
    return "1.0.0";
}

__attribute__((constructor))
void level2_constructor(void) {
    printf("[Level2Framework:Constructor] Level 2 loaded\n");
    printf("[Level2Framework:Constructor] Version: %s\n", level2_version());
    level2_init();
}

__attribute__((destructor))
void level2_destructor(void) {
    printf("[Level2Framework:Destructor] Cleanup\n");
}
