#ifndef MULTI_RPATH_RESOLVER_H
#define MULTI_RPATH_RESOLVER_H

#include <stdbool.h>
#include <stdlib.h>

/**
 * 다중 RPATH 환경 분석 모듈
 * 
 * Type C의 복잡한 RPATH 체인 처리 (깊이: 5~10단계)
 * 
 * 핵심 개념:
 * - RPATH 우선순위: LC_RPATH 로드 커맨드 순서대로 처리
 * - RPATH 미스: 경로에 파일이 없으면 다음 RPATH로 진행
 * - 취약점 판정: 후속 RPATH가 SIP 미보호 경로일 때 취약성 발생
 * 
 * 사용 예:
 * 1. RPATH 체인에서 dylib 추적
 *    size_t found_level = 0;
 *    RPATHResolutionStep *steps = resolve_rpath_chain_priority(
 *        binary_path, dylib_name, rpath_array, rpath_count, &found_level
 *    );
 * 
 * 2. 우선순위 매트릭스 생성
 *    RPATHPriorityMatrix *matrix = analyze_rpath_priority(
 *        binary_path, rpath_array, rpath_count, dylib_name
 *    );
 *    print_rpath_priority_matrix(matrix);
 */

#define RPATH_NOT_FOUND ((size_t)-1)

/**
 * RPATH 체인의 각 단계 정보
 */
typedef struct {
    size_t level;                   // RPATH 우선도 (0이 가장 높음)
    char *rpath_spec;               // RPATH 문자열 (예: "../Frameworks")
    char *resolved_path;            // 실제 해석된 경로
    bool exists;                    // 파일 존재 여부
} RPATHResolutionStep;

/**
 * RPATH 우선도 레벨의 상세 정보
 */
typedef struct {
    size_t level;                   // 우선도 (0이 가장 높음)
    char *rpath_spec;               // RPATH 문자열
    char *resolved_path;            // 해석된 경로
    bool file_exists;               // 파일 존재
    bool is_writable;               // 쓰기 가능
    bool is_sip_protected;          // SIP 보호 여부
    bool is_vulnerable;             // 취약점 판정
    int priority_score;             // 우선도 점수 (높을수록 취약)
} RPATHPriorityLevel;

/**
 * RPATH 우선순위 분석 결과 (매트릭스)
 */
typedef struct {
    char *binary_path;              // 분석 대상 바이너리
    char *target_dylib;             // 추적한 dylib 이름
    size_t rpath_count;             // RPATH 개수
    RPATHPriorityLevel *priority_levels;  // 각 레벨 정보
} RPATHPriorityMatrix;

/**
 * 의존성 체인의 한 노드
 */
typedef struct ChainNode {
    int depth;                      // 체인 깊이
    char *dylib_name;               // dylib 이름
    char *resolved_path;            // 해석된 경로
    size_t resolved_at_rpath_level; // 어느 RPATH 레벨에서 찾았나
    struct ChainNode **dependencies;  // 이 dylib이 의존하는 것들
    size_t dep_count;
} ChainNode;

/**
 * 전체 의존성 체인 분석 결과
 */
typedef struct {
    char *root_binary;              // 분석 시작점
    int max_depth;                  // 분석 최대 깊이
    ChainNode *chain;               // 의존성 체인
    size_t chain_length;            // 체인 길이
} DependencyChainAnalysis;

/* ================================================================
 * 공개 API
 * ================================================================ */

/**
 * RPATH 체인에서 dylib의 우선순위 기반 위치 결정
 * 
 * 동작:
 * 1. 첫 번째 RPATH에서 dylib 검색
 * 2. 없으면 두 번째 RPATH에서 검색
 * ... (RPATH 개수만큼 반복)
 * 3. 첫 발견 위치에서 멈춤
 * 
 * @param binary_path 바이너리 경로
 * @param dylib_name 찾을 dylib 이름
 * @param rpath_chain RPATH 배열 (우선순위 순)
 * @param rpath_count RPATH 개수
 * @param out_found_at_level 찾은 RPATH 레벨 (출력, 못 찾으면 RPATH_NOT_FOUND)
 * @return RPATHResolutionStep* 배열 (각 단계 정보)
 * 
 * 반환 배열은 호출자가 free_rpath_resolution_steps로 해제
 */
RPATHResolutionStep* resolve_rpath_chain_priority(
    const char *binary_path,
    const char *dylib_name,
    const char **rpath_chain,
    size_t rpath_count,
    size_t *out_found_at_level
);

/**
 * 의존성 체인 분석 (다단계 의존성 추적)
 * 
 * 동작:
 * 1. 메인 바이너리의 의존성 추출
 * 2. 각 의존성에 대해 동일 작업 반복
 * 3. max_depth까지 계속
 * 
 * Type C의 깊은 모듈 체인 분석에 사용
 * 
 * @param binary_path 분석 시작점
 * @param rpath_chain RPATH 배열
 * @param rpath_count RPATH 개수
 * @param max_depth 최대 분석 깊이
 * @return DependencyChainAnalysis* (실패 시 NULL)
 */
DependencyChainAnalysis* analyze_dependency_chain(
    const char *binary_path,
    const char **rpath_chain,
    size_t rpath_count,
    int max_depth
);

/**
 * RPATH 우선순위 분석 및 취약점 판정 매트릭스 생성
 * 
 * Type C의 핵심 분석: 모든 RPATH 레벨에 대한 상세 정보 수집
 * 
 * 분석 항목 (각 RPATH 레벨별):
 * - 파일 존재 여부
 * - 쓰기 가능 여부
 * - SIP 보호 여부
 * - 취약점 판정
 * - 우선도 점수 계산
 * 
 * @param binary_path 바이너리 경로
 * @param rpath_chain RPATH 배열 (우선순위 순)
 * @param rpath_count RPATH 개수
 * @param target_dylib_name 분석할 dylib 이름
 * @return RPATHPriorityMatrix* (실패 시 NULL)
 * 
 * 반환 구조는 호출자가 free_rpath_priority_matrix로 해제
 */
RPATHPriorityMatrix* analyze_rpath_priority(
    const char *binary_path,
    const char **rpath_chain,
    size_t rpath_count,
    const char *target_dylib_name
);

/**
 * RPATH 해석 단계 출력 (디버깅용)
 */
void print_rpath_resolution_steps(RPATHResolutionStep *steps);

/**
 * RPATH 우선순위 매트릭스 출력 (디버깅용)
 * 
 * 시각적으로 각 RPATH 레벨의 상태와 점수를 표시
 */
void print_rpath_priority_matrix(RPATHPriorityMatrix *matrix);

/**
 * RPATH 해석 단계 메모리 해제
 */
void free_rpath_resolution_steps(RPATHResolutionStep *steps);

/**
 * RPATH 우선순위 매트릭스 메모리 해제
 */
void free_rpath_priority_matrix(RPATHPriorityMatrix *matrix);

#endif // MULTI_RPATH_RESOLVER_H
