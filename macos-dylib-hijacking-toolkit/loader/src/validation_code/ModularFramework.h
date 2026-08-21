/**
 * ModularFramework.h
 * 
 * Type C 검증용 모듈화 Framework 헤더
 * 3단계 의존성 체인:
 * - Level1Framework
 * - Level2Framework (Level1 의존)
 * - Level3Framework (Level2 의존)
 */

#ifndef MODULAR_FRAMEWORK_H
#define MODULAR_FRAMEWORK_H

/* ================================================================
 * Level 1 Framework
 * ================================================================ */

void level1_init(void);
void level1_process(const char *data);
const char* level1_version(void);

__attribute__((constructor)) void level1_constructor(void);
__attribute__((destructor)) void level1_destructor(void);

/* ================================================================
 * Level 2 Framework
 * ================================================================ */

void level2_init(void);
void level2_process(const char *data);
const char* level2_version(void);

__attribute__((constructor)) void level2_constructor(void);
__attribute__((destructor)) void level2_destructor(void);

/* ================================================================
 * Level 3 Framework
 * ================================================================ */

void level3_init(void);
void level3_process(const char *data);
const char* level3_version(void);

__attribute__((constructor)) void level3_constructor(void);
__attribute__((destructor)) void level3_destructor(void);

#endif // MODULAR_FRAMEWORK_H
