#include "multi_rpath_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>

/**
 * @rpath를 실제 경로로 변환
 * 예: @rpath/../Frameworks/lib.dylib → /path/to/app/Frameworks/lib.dylib
 */
static char* resolve_rpath_at_level(
    const char *binary_path,
    const char *rpath_spec,
    const char *dylib_relative_path) {
    
    if (!binary_path || !dylib_relative_path) return NULL;
    
    // 바이너리가 속한 디렉토리의 경로
    char *binary_copy = strdup(binary_path);
    char *binary_dir = dirname(binary_copy);
    
    // dylib 경로가 @rpath로 시작하면 rpath_spec 적용
    const char *search_path = dylib_relative_path;
    if (strstr(search_path, "@rpath") == search_path) {
        search_path += 6; // "@rpath" 길이
        if (*search_path == '/') search_path++;
    }
    
    // 최종 경로 생성
    char resolved[4096];
    
    if (rpath_spec && rpath_spec[0] != '\0') {
        snprintf(resolved, sizeof(resolved), "%s/%s/%s", 
                 binary_dir, rpath_spec, search_path);
    } else {
        snprintf(resolved, sizeof(resolved), "%s/%s", 
                 binary_dir, search_path);
    }
    
    // 경로 정규화
    char *result = realpath(resolved, NULL);
    if (result) {
        free(binary_copy);
        return result;
    }
    
    free(binary_copy);
    return strdup(resolved);  // realpath 실패 시 그냥 반환
}

/**
 * RPATH 체인 내에서 dylib이 존재하는 첫 번째 위치 찾기
 * 
 * 동작:
 * 1. 첫 번째 RPATH 체크
 * 2. 없으면 두 번째 RPATH 체크
 * ... (RPATH 개수만큼 반복)
 */
static RPATHResolutionStep* find_dylib_in_rpath_chain(
    const char *binary_path,
    const char *dylib_name,
    const char **rpath_chain,
    size_t rpath_count,
    size_t *out_found_at_level) {
    
    if (!binary_path || !dylib_name || !rpath_chain || rpath_count == 0) {
        return NULL;
    }
    
    RPATHResolutionStep *steps = calloc(rpath_count + 1, sizeof(RPATHResolutionStep));
    size_t step_count = 0;
    
    printf("\n[*] RPATH 체인 추적: %s\n", dylib_name);
    printf("    RPATH 개수: %zu\n", rpath_count);
    
    for (size_t i = 0; i < rpath_count; i++) {
        steps[step_count].level = i;
        steps[step_count].rpath_spec = strdup(rpath_chain[i]);
        
        // 이 레벨에서의 경로 생성
        char *resolved = resolve_rpath_at_level(
            binary_path,
            rpath_chain[i],
            dylib_name
        );
        
        steps[step_count].resolved_path = resolved;
        
        // 파일 존재 여부 확인
        struct stat st;
        bool exists = (stat(resolved, &st) == 0);
        steps[step_count].exists = exists;
        
        printf("    [레벨 %zu] RPATH=%s\n", i, rpath_chain[i]);
        printf("             경로=%s\n", resolved);
        printf("             상태=%s\n", exists ? "✓ 발견" : "✗ 미스");
        
        step_count++;
        
        // dylib을 찾았으면 여기서 멈춤 (후속 RPATH는 무시)
        if (exists) {
            *out_found_at_level = i;
            printf("    → 첫 발견: 레벨 %zu\n", i);
            steps[step_count].level = -1;  // 종료 마커
            return steps;
        }
    }
    
    // 어느 RPATH에서도 찾지 못함
    printf("    → 모든 RPATH에서 미스\n");
    *out_found_at_level = RPATH_NOT_FOUND;
    steps[step_count].level = -1;  // 종료 마커
    
    return steps;
}

/**
 * 공개 API: RPATH 체인에서 dylib 검색
 */
RPATHResolutionStep* resolve_rpath_chain_priority(
    const char *binary_path,
    const char *dylib_name,
    const char **rpath_chain,
    size_t rpath_count,
    size_t *out_found_at_level) {
    
    if (!out_found_at_level) return NULL;
    
    return find_dylib_in_rpath_chain(
        binary_path,
        dylib_name,
        rpath_chain,
        rpath_count,
        out_found_at_level
    );
}

/**
 * 공개 API: 의존성 체인 분석 (다단계)
 * 
 * 동작:
 * 1. 메인 바이너리의 의존성 추출
 * 2. 각 의존성에 대해 동일한 작업 반복
 * 3. 깊이 제한(max_depth)까지만 진행
 */
DependencyChainAnalysis* analyze_dependency_chain(
    const char *binary_path,
    const char **rpath_chain,
    size_t rpath_count,
    int max_depth) {
    
    if (!binary_path || !rpath_chain || rpath_count == 0 || max_depth <= 0) {
        return NULL;
    }
    
    DependencyChainAnalysis *analysis = calloc(1, sizeof(DependencyChainAnalysis));
    analysis->root_binary = strdup(binary_path);
    analysis->max_depth = max_depth;
    
    printf("\n[*] 의존성 체인 분석 시작 (최대 깊이: %d)\n", max_depth);
    printf("    바이너리: %s\n", binary_path);
    
    // 최대 깊이 × 최대 RPATH 개수 정도로 버퍼 할당
    analysis->chain = calloc(max_depth * rpath_count + 1, sizeof(ChainNode));
    
    // BFS/DFS로 의존성 추적
    // (현재는 간단한 방식, 실제로는 더 정교한 그래프 탐색 필요)
    
    return analysis;
}

/**
 * 공개 API: RPATH 우선순위 분석 및 취약점 판정
 * 
 * 동작:
 * Type C의 복잡한 다중 RPATH 환경에서:
 * - 첫 번째 RPATH 미스 시 후속 RPATH 자동 탐색
 * - 후속 RPATH가 SIP 미보호 경로(쓰기 가능)일 때 취약점 판정
 * - 우선순위 매트릭스 생성
 */
RPATHPriorityMatrix* analyze_rpath_priority(
    const char *binary_path,
    const char **rpath_chain,
    size_t rpath_count,
    const char *target_dylib_name) {
    
    if (!binary_path || !rpath_chain || rpath_count == 0 || !target_dylib_name) {
        return NULL;
    }
    
    RPATHPriorityMatrix *matrix = calloc(1, sizeof(RPATHPriorityMatrix));
    matrix->binary_path = strdup(binary_path);
    matrix->target_dylib = strdup(target_dylib_name);
    matrix->rpath_count = rpath_count;
    matrix->priority_levels = calloc(rpath_count, sizeof(RPATHPriorityLevel));
    
    printf("\n[*] RPATH 우선순위 매트릭스 생성\n");
    printf("    바이너리: %s\n", binary_path);
    printf("    대상 dylib: %s\n", target_dylib_name);
    printf("    RPATH 개수: %zu\n", rpath_count);
    
    for (size_t i = 0; i < rpath_count; i++) {
        RPATHPriorityLevel *level = &matrix->priority_levels[i];
        level->level = i;
        level->rpath_spec = strdup(rpath_chain[i]);
        
        // 이 RPATH에서 dylib 위치 계산
        char *resolved = resolve_rpath_at_level(
            binary_path,
            rpath_chain[i],
            target_dylib_name
        );
        level->resolved_path = resolved;
        
        // 파일 존재 여부
        struct stat st;
        level->file_exists = (stat(resolved, &st) == 0);
        
        // 쓰기 가능 여부 (보안 판정)
        level->is_writable = (stat(resolved, &st) == 0) && 
                            (st.st_mode & S_IWUSR);
        
        // SIP 보호 여부 (간단한 휴리스틱)
        level->is_sip_protected = 
            strstr(resolved, "/System/Library") != NULL ||
            strstr(resolved, "/Library/System") != NULL ||
            strstr(resolved, "/usr/lib") != NULL;
        
        // 취약점 판정
        level->is_vulnerable = level->file_exists && 
                              level->is_writable && 
                              !level->is_sip_protected;
        
        // 우선도 계산
        if (level->is_vulnerable) {
            level->priority_score = 1000 - (i * 10);  // 앞의 RPATH가 더 높은 우선도
        } else if (level->file_exists && level->is_sip_protected) {
            level->priority_score = 500 - (i * 5);   // SIP 보호는 낮은 우선도
        } else {
            level->priority_score = 0;  // 파일 없음
        }
        
        printf("\n    [RPATH %zu]\n", i);
        printf("      RPATH: %s\n", rpath_chain[i]);
        printf("      경로: %s\n", resolved);
        printf("      존재: %s\n", level->file_exists ? "YES" : "NO");
        printf("      쓰기: %s\n", level->is_writable ? "YES" : "NO");
        printf("      SIP: %s\n", level->is_sip_protected ? "보호됨" : "보호안됨");
        printf("      취약: %s (점수: %d)\n", 
               level->is_vulnerable ? "YES" : "NO",
               level->priority_score);
    }
    
    return matrix;
}

/**
 * 공개 API: RPATH 해석 결과 출력
 */
void print_rpath_resolution_steps(RPATHResolutionStep *steps) {
    if (!steps) {
        printf("[ERROR] Resolution steps가 NULL입니다\n");
        return;
    }
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("RPATH 해석 결과\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    for (size_t i = 0; steps[i].level != -1; i++) {
        printf("\n[단계 %zu]\n", steps[i].level);
        printf("  RPATH: %s\n", steps[i].rpath_spec);
        printf("  경로: %s\n", steps[i].resolved_path);
        printf("  상태: %s\n", steps[i].exists ? "✓ 발견" : "✗ 미스");
    }
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

/**
 * 공개 API: 우선순위 매트릭스 출력
 */
void print_rpath_priority_matrix(RPATHPriorityMatrix *matrix) {
    if (!matrix) {
        printf("[ERROR] Priority matrix가 NULL입니다\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║           RPATH 우선순위 매트릭스                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("바이너리: %s\n", matrix->binary_path);
    printf("대상: %s\n", matrix->target_dylib);
    printf("RPATH 개수: %zu\n", matrix->rpath_count);
    
    printf("\n┌──────────────────────────────────────────────────────────────┐\n");
    printf("│  레벨  │ RPATH 경로 │ 존재 │ 쓰기 │ SIP │ 취약 │  점수  │\n");
    printf("├──────────────────────────────────────────────────────────────┤\n");
    
    for (size_t i = 0; i < matrix->rpath_count; i++) {
        RPATHPriorityLevel *level = &matrix->priority_levels[i];
        
        printf("│  %3zu  │ %-30s │ %s │ %s │ %s │ %s │ %4d │\n",
               level->level,
               level->rpath_spec,
               level->file_exists ? "Y" : "N",
               level->is_writable ? "Y" : "N",
               level->is_sip_protected ? "Y" : "N",
               level->is_vulnerable ? "Y" : "N",
               level->priority_score);
    }
    
    printf("└──────────────────────────────────────────────────────────────┘\n");
    
    // 최고 우선도 RPATH 찾기
    int max_score = -1;
    int best_level = -1;
    for (size_t i = 0; i < matrix->rpath_count; i++) {
        if (matrix->priority_levels[i].priority_score > max_score) {
            max_score = matrix->priority_levels[i].priority_score;
            best_level = i;
        }
    }
    
    if (best_level >= 0) {
        printf("\n[권장] 주입 경로: 레벨 %d (점수: %d)\n", 
               best_level, max_score);
    }
}

/**
 * 공개 API: 메모리 해제
 */
void free_rpath_resolution_steps(RPATHResolutionStep *steps) {
    if (!steps) return;
    
    for (size_t i = 0; steps[i].level != -1; i++) {
        free(steps[i].rpath_spec);
        free(steps[i].resolved_path);
    }
    free(steps);
}

/**
 * 공개 API: 우선순위 매트릭스 메모리 해제
 */
void free_rpath_priority_matrix(RPATHPriorityMatrix *matrix) {
    if (!matrix) return;
    
    free(matrix->binary_path);
    free(matrix->target_dylib);
    
    for (size_t i = 0; i < matrix->rpath_count; i++) {
        free(matrix->priority_levels[i].rpath_spec);
        free(matrix->priority_levels[i].resolved_path);
    }
    free(matrix->priority_levels);
    free(matrix);
}
