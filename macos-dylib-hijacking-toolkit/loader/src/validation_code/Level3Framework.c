/**
 * Level3Framework.c
 * 
 * Type C 검증용 Level 3 Framework (최하위)
 * Level2Framework에 의존
 */

#include <stdio.h>
#include <stdlib.h>
#include "ModularFramework.h"

/* ================================================================
 * Level 3 Framework 구현
 * ================================================================ */

void level3_init(void) {
    printf("[Level3Framework] Initializing...\n");
}

void level3_process(const char *data) {
    if (!data) return;
    printf("[Level3Framework] Processing: %s (final processing)\n", data);
}

const char* level3_version(void) {
    return "1.0.0";
}

__attribute__((constructor))
void level3_constructor(void) {
    printf("[Level3Framework:Constructor] Level 3 loaded\n");
    printf("[Level3Framework:Constructor] Version: %s\n", level3_version());
    level3_init();
}

__attribute__((destructor))
void level3_destructor(void) {
    printf("[Level3Framework:Destructor] Cleanup\n");
}
