/**
 * SimplePayload.c
 * 
 * Type B 검증용 예제 Framework 구현
 * 정상 동작하는 Framework
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SimplePayload.h"

/* ================================================================
 * 정상 Framework 함수
 * ================================================================ */

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

/* ================================================================
 * Constructor/Destructor
 * ================================================================ */

__attribute__((constructor))
void simple_payload_constructor(void) {
    printf("[SimplePayload] Constructor called - Framework initialization\n");
    printf("[SimplePayload] Version: %s\n", simple_payload_version());
    simple_payload_init();
}

__attribute__((destructor))
void simple_payload_destructor(void) {
    printf("[SimplePayload] Destructor called - Framework cleanup\n");
}
