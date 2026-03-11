#ifndef RPATH_INJECTOR_H
#define RPATH_INJECTOR_H

#include "result_parser.h"
#include <stdbool.h>

/**
 * RPATH 취약점 주입 전략 관련 함수들
 */

/**
 * RPATH 취약 경로 분석 및 주입 가능한 위치 찾기
 * 
 * Scanner에서 발견한 RPATH 취약 경로들을 분석하여
 * 실제로 dylib을 배치할 수 있는 위치를 결정합니다.
 * 
 * @param binary_path 취약한 바이너리의 경로
 * @param rpath_vulns RPATH 취약 경로 배열
 * @param rpath_count RPATH 취약 경로 개수
 * @param selected_index 사용자가 선택한 dylib의 인덱스 (0부터 시작)
 * @param out_injection_path 주입할 경로 (동적 할당, 호출자가 해제)
 * @return 성공 시 true, 실패 시 false
 */
bool analyze_rpath_vulnerability(
    const char *binary_path,
    char **rpath_vulns,
    size_t rpath_count,
    size_t selected_index,
    char **out_injection_path
);

/**
 * RPATH 취약점을 통한 dylib 주입
 * 
 * 1. 사용자 dylib을 주입 경로로 복사
 * 2. dylib의 install_name을 원본 dylib 경로로 설정
 * 3. LC_REEXPORT_DYLIB로 원본 dylib 재수출 추가
 * 
 * @param source_dylib 사용자가 제공한 dylib 경로
 * @param binary_path 취약한 바이너리 경로
 * @param rpath_vulns RPATH 취약 경로 배열
 * @param rpath_count RPATH 취약 경로 개수
 * @param original_dylib_name 원본 dylib 이름 (예: libswiftCore.dylib)
 * @param selected_rpath_index 사용자가 선택한 dylib의 인덱스 (0부터 시작)
 * @return 성공 시 true, 실패 시 false
 */
bool inject_via_rpath(
    const char *source_dylib,
    const char *binary_path,
    char **rpath_vulns,
    size_t rpath_count,
    const char *original_dylib_name,
    size_t selected_rpath_index
);

#endif // RPATH_INJECTOR_H
