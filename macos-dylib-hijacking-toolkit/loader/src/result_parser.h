#ifndef RESULT_PARSER_H
#define RESULT_PARSER_H

#include <stdbool.h>
#include <stdlib.h>

/**
 * 취약점 타입
 */
typedef enum {
    VULN_NONE = 0,
    VULN_RPATH = 1,
    VULN_WEAK_DYLIB = 2,
    VULN_BOTH = 3
} VulnerabilityType;

/**
 * 파싱된 취약점 정보 (로더용)
 */
typedef struct {
    char *binary_path;              // 취약한 바이너리 경로 (정규화됨)
    VulnerabilityType vuln_type;    // VULN_RPATH | VULN_WEAK_DYLIB | VULN_BOTH

    /* RPATH 취약 경로들 (정규화된 절대경로) */
    char **rpath_vulns;
    size_t rpath_count;
    size_t rpath_cap;               // 동적 배열 용량

    /* Weak Dylib 취약 경로들 */
    char **weak_dylib_vulns;
    size_t weak_dylib_count;
    size_t weak_dylib_cap;          // 동적 배열 용량 (기존 weak_dylib_vulns -> weak_dylib_cap으로 수정)
} VulnerableTarget;

/**
 * 파싱 결과 전체
 */
typedef struct {
    VulnerableTarget **targets;
    size_t target_count;
    size_t capacity;                // targets 배열 용량
} ParsedResults;

/* ------------------------------------------------------------
 * 공개 API
 * ------------------------------------------------------------ */

/**
 * Scanner 결과 파일(리포트) 파싱
 * @param output_file  Scanner가 생성한 report.txt 경로
 * @return ParsedResults* (실패 시 NULL)
 */
ParsedResults *parse_scanner_output(const char *output_file);

/**
 * 파싱 결과 메모리 해제
 */
void free_parsed_results(ParsedResults *results);

/**
 * 파싱 결과 콘솔 출력 (디버깅용)
 */
void print_parsed_results(ParsedResults *results);

#endif // RESULT_PARSER_H