/**
 * framework_injector.h
 * 
 * Type B (Framework) 전문화 주입 모듈
 * - Framework 번들 기반 주입
 * - Reexport 처리
 */

#ifndef FRAMEWORK_INJECTOR_H
#define FRAMEWORK_INJECTOR_H

#include <stdbool.h>
#include <stdlib.h>

/* ================================================================
 * Type B 주입 컨텍스트
 * ================================================================ */

/**
 * Framework 번들에 대한 주입 계획
 */
typedef struct {
    char *framework_path;           // Framework 번들 경로
    char *framework_name;           // Framework 이름 (예: "Sparkle")
    
    // 주입 대상
    char *target_dylib;             // 주입할 dylib 경로
    char *injection_location;       // Framework 내 주입 위치
    
    // 주입 방법
    bool use_reexport;              // re-export 방식 사용 여부
    bool requires_codesign;         // 코드 서명 필요 여부
    
    // 메타데이터
    size_t affected_app_count;      // 영향받을 앱 개수
    char *impact_summary;           // 영향도 요약
} FrameworkInjectionPlan;

/**
 * Framework 주입 결과
 */
typedef struct {
    bool success;                   // 주입 성공 여부
    
    // 주입된 Framework
    char *framework_path;
    
    // 검증 결과
    size_t successfully_injected_apps;
    size_t failed_apps;
    char **failed_app_paths;
    
    // 메트릭
    double injection_time_ms;       // 주입 소요 시간
    char *log_message;              // 상세 로그
} FrameworkInjectionResult;

/* ================================================================
 * 공개 API
 * ================================================================ */

/**
 * Framework 주입 계획 생성
 * 
 * @param framework_path: Framework 번들 경로
 * @param target_dylib: 주입할 dylib 경로
 * @return: 생성된 FrameworkInjectionPlan, 또는 NULL if error
 */
FrameworkInjectionPlan* create_framework_injection_plan(
    const char *framework_path,
    const char *target_dylib
);

/**
 * Framework 내에서 최적 주입 위치 결정
 * 
 * @param framework_path: Framework 번들 경로
 * @param target_dylib: 주입할 dylib 이름
 * @return: Framework 내 주입 위치 경로
 */
char* determine_framework_injection_location(
    const char *framework_path,
    const char *target_dylib
);

/**
 * Re-export 가능 여부 확인
 * 
 * @param framework_path: Framework 번들 경로
 * @return: true if re-export 가능
 * 
 * Framework의 Umbrella 헤더 등을 확인하여
 * re-export 방식 사용 가능 여부 판정
 */
bool can_use_reexport(const char *framework_path);

/**
 * Framework 주입 실행
 * 
 * @param plan: FrameworkInjectionPlan
 * @return: 주입 결과 (FrameworkInjectionResult)
 */
FrameworkInjectionResult* execute_framework_injection(
    FrameworkInjectionPlan *plan
);

/**
 * Framework 주입 가능성 검증
 * 
 * @param framework_path: Framework 경로
 * @return: true if 주입 가능
 */
bool validate_framework_injectability(const char *framework_path);

/**
 * 주입 계획 정보 출력
 * 
 * @param plan: FrameworkInjectionPlan
 */
void print_framework_injection_plan(FrameworkInjectionPlan *plan);

/**
 * 주입 결과 정보 출력
 * 
 * @param result: FrameworkInjectionResult
 */
void print_framework_injection_result(FrameworkInjectionResult *result);

/**
 * 메모리 해제 - 계획
 * 
 * @param plan: FrameworkInjectionPlan (포인터의 포인터)
 */
void free_framework_injection_plan(FrameworkInjectionPlan **plan);

/**
 * 메모리 해제 - 결과
 * 
 * @param result: FrameworkInjectionResult (포인터의 포인터)
 */
void free_framework_injection_result(FrameworkInjectionResult **result);

#endif // FRAMEWORK_INJECTOR_H
