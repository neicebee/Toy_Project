/**
 * dependency_chain.h
 * 
 * macOS dylib 의존성 그래프 분석
 * - 의존성 체인 구성
 * - 순환 참조 감지
 * - 최적 주입 경로 결정
 * - RPATH 기반 해석
 */

#ifndef DEPENDENCY_CHAIN_H
#define DEPENDENCY_CHAIN_H

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

/* ================================================================
 * 의존성 노드 구조체
 * ================================================================ */

/**
 * 의존성 그래프의 단일 노드
 */
typedef struct DependencyNode {
    char *dylib_name;               // dylib 이름 (예: "libsoxr.0.dylib")
    char *resolved_path;            // RPATH 해석된 최종 경로
    char *load_command_spec;        // 원본 load command 명령어
    
    // 의존성 관계
    struct DependencyNode **dependencies;  // 이 dylib이 의존하는 다른 dylib들
    size_t dep_count;
    
    struct DependencyNode **dependents;    // 이 dylib에 의존하는 dylib들
    size_t dependent_count;
    
    // 그래프 분석 정보
    int visit_state;                // 0=미방문, 1=방문중, 2=완료 (순환참조 감지용)
    int graph_depth;                // 그래프에서의 깊이 (0=루트)
    bool is_injectable;             // 주입 가능 여부
    bool is_system_lib;             // 시스템 라이브러리 여부
    bool is_framework;              // Framework 번들 여부
    
    // 주입 우선도 점수
    int injection_priority;         // 높을수록 우선순위 높음
    time_t last_modified;           // 파일 수정 시간
} DependencyNode;

/**
 * 의존성 그래프 전체
 */
typedef struct {
    DependencyNode **nodes;         // 모든 노드
    size_t node_count;
    
    // 그래프 분석 결과
    bool has_circular_dependency;   // 순환 참조 존재 여부
    char **cycle_nodes;             // 순환참조 노드들
    size_t cycle_count;
    
    // 분석 컨텍스트
    char *binary_path;              // 분석 대상 바이너리 경로
    char *app_bundle_path;          // 앱 번들 경로
    
    // RPATH 정보
    char **rpath_list;              // 바이너리의 RPATH 목록
    size_t rpath_count;
} DependencyGraph;

/* ================================================================
 * 공개 API
 * ================================================================ */

/**
 * 의존성 그래프 생성
 * 
 * @param binary_path: 분석 대상 바이너리 경로
 * @param app_bundle_path: 앱 번들 경로 (RPATH 해석용)
 * @return: 생성된 DependencyGraph 포인터, 또는 NULL 실패 시
 */
DependencyGraph* build_dependency_graph(
    const char *binary_path,
    const char *app_bundle_path
);

/**
 * 순환 참조 감지
 * 
 * @param graph: 분석할 DependencyGraph
 * @return: true if 순환 참조 있음, false otherwise
 */
bool detect_circular_dependency(DependencyGraph *graph);

/**
 * 최적 주입 경로 찾기
 * 
 * @param graph: DependencyGraph
 * @param target_dylib: 대상 dylib 이름 (예: "libsoxr.0.dylib")
 * @return: 최적 주입 경로, 또는 NULL if not found
 * 
 * 기준:
 * 1. RPATH 우선도 가장 높은 곳
 * 2. 의존성 충돌 없는 곳
 * 3. 접근 권한이 있는 곳
 */
char* find_best_injection_path(
    DependencyGraph *graph,
    const char *target_dylib
);

/**
 * 주입 가능한 모든 경로 찾기
 * 
 * @param graph: DependencyGraph
 * @param target_dylib: 대상 dylib 이름
 * @param out_count: 반환된 경로 개수 저장
 * @return: 경로 배열 (우선도순), 또는 NULL
 * 
 * Type B/C 분석에서 모든 가능한 주입 지점을 찾을 때 사용
 */
char** find_all_injectable_paths(
    DependencyGraph *graph,
    const char *target_dylib,
    size_t *out_count
);

/**
 * 의존성 체인 길이 계산
 * 
 * @param graph: DependencyGraph
 * @return: 가장 긴 의존성 체인의 길이
 */
size_t calculate_dependency_chain_length(DependencyGraph *graph);

/**
 * 특정 dylib이 영향받는 범위 계산
 * 
 * @param graph: DependencyGraph
 * @param dylib_name: dylib 이름
 * @param out_count: 영향받는 dylib 개수
 * @return: 영향받는 dylib 이름 배열
 */
char** get_affected_dependents(
    DependencyGraph *graph,
    const char *dylib_name,
    size_t *out_count
);

/**
 * 그래프 정보 출력
 * 
 * @param graph: DependencyGraph
 */
void print_dependency_graph(DependencyGraph *graph);

/**
 * 그래프 노드 상세 정보 출력
 * 
 * @param node: DependencyNode
 * @param indent: 들여쓰기 수준
 */
void print_dependency_node(DependencyNode *node, int indent);

/**
 * 메모리 해제
 * 
 * @param graph: DependencyGraph (포인터를 포인터로 받아 NULL 처리)
 */
void free_dependency_graph(DependencyGraph **graph);

/**
 * 단일 노드 메모리 해제
 * 
 * @param node: DependencyNode
 */
void free_dependency_node(DependencyNode *node);

#endif // DEPENDENCY_CHAIN_H
