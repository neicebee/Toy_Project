#ifndef WEAK_DYLIB_INJECTOR_H
#define WEAK_DYLIB_INJECTOR_H

#include "result_parser.h"
#include <stdbool.h>

/**
 * Weak Dylib 취약점 주입 전략 관련 함수들
 */

/**
 * Weak Dylib 취약 경로 분석 및 주입 가능한 위치 찾기
 * 
 * Scanner에서 발견한 Weak Dylib 취약 경로들을 분석하여
 * 실제로 dylib을 배치할 수 있는 위치를 결정합니다.
 * 
 * @param binary_path 취약한 바이너리의 경로
 * @param weak_dylibs Weak Dylib 취약 경로 배열
 * @param weak_dylib_count Weak Dylib 취약 경로 개수
 * @param out_injection_path 주입할 경로 (동적 할당, 호출자가 해제)
 * @return 성공 시 true, 실패 시 false
 */
bool analyze_weak_dylib_vulnerability(
    const char *binary_path,
    char **weak_dylibs,
    size_t weak_dylib_count,
    char **out_injection_path
);

/**
 * Weak Dylib 취약점을 통한 dylib 주입
 * 
 * Weak dylib은 로드 실패해도 계속 실행되는 라이브러리입니다.
 * 이 특성을 이용해 공격자가 제어하는 위치에 dylib을 배치합니다.
 * 
 * 동작 과정:
 * 1. 취약한 Weak Dylib 경로를 파악
 * 2. 해당 경로에 사용자 dylib을 배치
 * 3. dylib의 install_name을 원본 dylib 경로로 설정
 * 
 * @param source_dylib 사용자가 제공한 dylib 경로
 * @param binary_path 취약한 바이너리 경로
 * @param weak_dylibs Weak Dylib 취약 경로 배열
 * @param weak_dylib_count Weak Dylib 취약 경로 개수
 * @param original_dylib_name 원본 dylib 이름 (예: libswiftCoreFoundation.dylib)
 * @return 성공 시 true, 실패 시 false
 */
bool inject_via_weak_dylib(
    const char *source_dylib,
    const char *binary_path,
    char **weak_dylibs,
    size_t weak_dylib_count,
    const char *original_dylib_name
);

#endif // WEAK_DYLIB_INJECTOR_H
