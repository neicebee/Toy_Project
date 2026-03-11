#ifndef RESULT_PARSER_H
#define RESULT_PARSER_H

#include <stdbool.h>
#include <stdlib.h>

/**
 * 취약점 타입
 * RPATH_VULNERABLE: @rpath 경로 취약점
 * WEAK_DYLIB_VULNERABLE: Weak Dylib 경로 취약점
 * BOTH_VULNERABLE: 두 가지 모두 존재
 */
typedef enum {
    VULN_NONE = 0,
    VULN_RPATH = 1,
    VULN_WEAK_DYLIB = 2,
    VULN_BOTH = 3
} VulnerabilityType;

/**
 * 파싱된 취약점 정보
 */
typedef struct {
    char *binary_path;              // 취약한 바이너리의 경로
    VulnerabilityType vuln_type;    // 취약점 타입
    char **rpath_vulns;             // RPATH 취약 경로 배열
    size_t rpath_count;             // RPATH 취약 경로 개수
    char **weak_dylib_vulns;        // Weak Dylib 취약 경로 배열
    size_t weak_dylib_count;        // Weak Dylib 취약 경로 개수
} VulnerableTarget;

/**
 * Scanner 결과 파싱 결과 배열
 */
typedef struct {
    VulnerableTarget **targets;     // 취약한 바이너리들 (포인터 배열)
    size_t target_count;            // 취약한 바이너리 개수
} ParsedResults;

/**
 * Scanner 결과 파일을 읽고 파싱합니다.
 * 
 * @param output_file Scanner 결과 파일 경로 (output.txt의 경로)
 * @return 파싱된 결과. 실패 시 NULL 반환.
 */
ParsedResults* parse_scanner_output(const char *output_file);

/**
 * 파싱된 결과를 해제합니다.
 * 
 * @param results parse_scanner_output에서 반환된 결과
 */
void free_parsed_results(ParsedResults *results);

/**
 * 파싱된 결과를 출력합니다 (디버깅용).
 * 
 * @param results 파싱된 결과
 */
void print_parsed_results(ParsedResults *results);

#endif // RESULT_PARSER_H
