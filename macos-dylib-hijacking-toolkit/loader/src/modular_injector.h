/**
 * modular_injector.h
 * 
 * Type C (모듈화 체인) 전문화 주입 모듈
 * - 모든 진입점 추출 (main, XPC, Widget)
 * - 모듈화 의존성 트리 구성
 * - 다중 진입점 동시 주입
 */

#ifndef MODULAR_INJECTOR_H
#define MODULAR_INJECTOR_H

#include <stdbool.h>
#include <stdlib.h>

/* ================================================================
 * 진입점 정보
 * ================================================================ */

/**
 * 앱의 진입점 (main binary, XPC service, Widget extension 등)
 */
typedef struct {
    char *entry_point_type;     // "main", "xpc_service", "widget", "app_extension"
    char *binary_path;          // 바이너리 경로
    char *display_name;         // 표시 이름 (예: "HandBrakeXPCService2")
    
    // RPATH 정보
    char **rpath_list;          // 이 진입점의 RPATH 목록
    size_t rpath_count;
    
    // 의존성
    char **framework_deps;      // 의존하는 Framework들
    size_t framework_dep_count;
    
    // 상태
    bool is_injectable;         // 주입 가능 여부
    int depth_in_chain;         // 의존성 체인 깊이
} EntryPoint;

/**
 * 모듈화 앱의 전체 주입 계획
 */
typedef struct {
    char *app_bundle_path;      // 앱 번들 경로
    char *app_name;             // 앱 이름
    
    // 진입점들
    EntryPoint **entry_points;  // 모든 진입점
    size_t entry_point_count;
    
    // 모듈화 정보
    char **internal_frameworks;  // 앱 내부 Framework들
    size_t internal_framework_count;
    
    // 공유 Framework들 (Type B 요소)
    char **shared_frameworks;    // 다른 앱과 공유하는 Framework들
    size_t shared_framework_count;
    
    // 의존성 체인
    size_t max_dependency_depth;  // 최대 의존성 깊이
    
    // 주입 전략
    char *injection_strategy;    // "broadcast" / "selective" / "cascading"
    bool use_dependency_graph;   // 의존성 그래프 기반 결정 사용
    
    // 메타데이터
    size_t total_affected_apps;  // 영향받는 앱 (내부 + 공유 Framework)
    char *complexity_assessment; // 복잡도 평가
} ModularInjectionPlan;

/**
 * 모듈화 주입 결과
 */
typedef struct {
    // 전체 결과
    bool success;               // 전체 주입 성공 여부
    size_t total_entry_points;
    size_t successfully_injected;
    size_t failed_entry_points;
    
    // 진입점별 결과
    bool *entry_point_results;  // 각 진입점 주입 성공 여부
    char **failed_entry_paths;
    
    // 메트릭
    double total_injection_time_ms;
    double *per_entry_point_time_ms;
} ModularInjectionResult;

/* ================================================================
 * 공개 API
 * ================================================================ */

/**
 * 앱의 모든 진입점 추출
 * 
 * @param app_bundle_path: 앱 번들 경로
 * @param out_count: 반환된 진입점 개수
 * @return: EntryPoint 배열
 * 
 * main, XPCServices, PlugIns 등 모든 진입점 찾기
 */
EntryPoint** extract_all_entry_points(
    const char *app_bundle_path,
    size_t *out_count
);

/**
 * 모듈화 의존성 트리 구성
 *
 * @param entry_points: 진입점 배열
 * @param entry_point_count: 진입점 개수
 * @return: 구성된 의존성 정보 (문자열)
 */
char* build_modular_dependency_tree(
    EntryPoint **entry_points,
    size_t entry_point_count
);

/**
 * 모듈화 주입 계획 생성
 * 
 * @param app_bundle_path: 앱 번들 경로
 * @param target_dylib: 주입할 dylib 이름
 * @return: ModularInjectionPlan 구조체
 */
ModularInjectionPlan* create_modular_injection_plan(
    const char *app_bundle_path,
    const char *target_dylib
);

/**
 * 각 진입점별 최적 주입 위치 결정
 * 
 * @param plan: ModularInjectionPlan
 * @return: 진입점별 최적 위치 배열 (문자열 배열)
 */
char** determine_injection_points_per_entry(
    ModularInjectionPlan *plan
);

/**
 * 모듈화 주입 복잡도 평가
 * 
 * @param plan: ModularInjectionPlan
 * @return: 복잡도 평가 문자열
 * 
 * "높음 (11개 Framework, 70개 RPATH, 5개 진입점)"
 */
char* assess_modular_complexity(ModularInjectionPlan *plan);

/**
 * 모듈화 주입 실행
 * 
 * @param plan: ModularInjectionPlan
 * @return: 주입 결과
 */
ModularInjectionResult* execute_modular_injection(
    ModularInjectionPlan *plan
);

/**
 * 모든 진입점 검증
 * 
 * @param plan: ModularInjectionPlan
 * @param result: ModularInjectionResult (주입 결과)
 * @return: true if 모든 진입점 검증 성공
 */
bool validate_all_entry_points(
    ModularInjectionPlan *plan,
    ModularInjectionResult *result
);

/**
 * 주입 계획 정보 출력
 * 
 * @param plan: ModularInjectionPlan
 */
void print_modular_injection_plan(ModularInjectionPlan *plan);

/**
 * 주입 결과 정보 출력
 * 
 * @param result: ModularInjectionResult
 */
void print_modular_injection_result(ModularInjectionResult *result);

/**
 * 진입점 정보 출력
 * 
 * @param ep: EntryPoint
 * @param indent: 들여쓰기 수준
 */
void print_entry_point(EntryPoint *ep, int indent);

/**
 * 메모리 해제 - 진입점
 * 
 * @param entry_points: EntryPoint 배열 (포인터의 포인터)
 * @param count: 배열 크기
 */
void free_entry_points(EntryPoint ***entry_points, size_t count);

/**
 * 메모리 해제 - 계획
 * 
 * @param plan: ModularInjectionPlan (포인터의 포인터)
 */
void free_modular_injection_plan(ModularInjectionPlan **plan);

/**
 * 메모리 해제 - 결과
 * 
 * @param result: ModularInjectionResult (포인터의 포인터)
 */
void free_modular_injection_result(ModularInjectionResult **result);

#endif // MODULAR_INJECTOR_H
