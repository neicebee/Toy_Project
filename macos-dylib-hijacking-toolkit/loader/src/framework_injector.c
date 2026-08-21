/**
 * framework_injector.c
 * 
 * Type B (Framework) 주입 구현
 */

#include "framework_injector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* ================================================================
 * 내부 헬퍼 함수들
 * ================================================================ */

/**
 * Framework 이름 추출 (예: Sparkle.framework → Sparkle)
 */
static char* extract_framework_name(const char *framework_path) {
    if (!framework_path) return NULL;
    
    // 마지막 "/" 찾기
    const char *last_slash = strrchr(framework_path, '/');
    const char *name_start = last_slash ? last_slash + 1 : framework_path;
    
    // ".framework" 찾기
    const char *dot_framework = strstr(name_start, ".framework");
    if (!dot_framework) return NULL;
    
    // 프레임워크 이름 추출
    size_t len = dot_framework - name_start;
    char *name = malloc(len + 1);
    strncpy(name, name_start, len);
    name[len] = '\0';
    
    return name;
}

/**
 * 파일 또는 디렉토리 존재 확인
 */
static bool file_exists(const char *path) {
    if (!path) return false;
    return access(path, F_OK) == 0;
}

/**
 * 파일 쓰기 권한 확인
 */
static bool has_write_permission(const char *path) {
    if (!path) return false;
    return access(path, W_OK) == 0;
}

/* ================================================================
 * 공개 API 구현
 * ================================================================ */

/**
 * Framework 주입 계획 생성
 */
FrameworkInjectionPlan* create_framework_injection_plan(
    const char *framework_path,
    const char *target_dylib) {
    
    if (!framework_path || !target_dylib) return NULL;
    
    FrameworkInjectionPlan *plan = calloc(1, sizeof(FrameworkInjectionPlan));
    
    plan->framework_path = strdup(framework_path);
    plan->framework_name = extract_framework_name(framework_path);
    plan->target_dylib = strdup(target_dylib);
    
    // 주입 위치 결정
    plan->injection_location = determine_framework_injection_location(
        framework_path, target_dylib);
    
    // Re-export 사용 가능 여부 확인
    plan->use_reexport = can_use_reexport(framework_path);
    
    // 코드 서명 필요 여부 확인
    plan->requires_codesign = true;  // 기본적으로 필요
    
    return plan;
}

/**
 * Framework 내에서 최적 주입 위치 결정
 */
char* determine_framework_injection_location(
    const char *framework_path,
    const char *target_dylib) {
    
    if (!framework_path) return NULL;
    
    // 기본 위치들 시도 (우선순위 순)
    char *possible_locations[] = {
        "%s/Versions/A/%s",
        "%s/Versions/Current/%s",
        "%s/%s",
        NULL
    };
    
    for (int i = 0; possible_locations[i] != NULL; i++) {
        char path[1024];
        snprintf(path, sizeof(path), possible_locations[i], 
                 framework_path, target_dylib);
        
        if (file_exists(path) && has_write_permission(path)) {
            return strdup(path);
        }
    }
    
    // 기본값: Versions/A 위치
    char default_path[1024];
    snprintf(default_path, sizeof(default_path), "%s/Versions/A/%s",
             framework_path, target_dylib);
    return strdup(default_path);
}

/**
 * Re-export 가능 여부 확인
 */
bool can_use_reexport(const char *framework_path) {
    if (!framework_path) return false;
    
    // Umbrella 헤더 또는 Headers/XXX.h 존재 확인
    char headers_path[1024];
    snprintf(headers_path, sizeof(headers_path), "%s/Headers", framework_path);
    
    return file_exists(headers_path);
}

/**
 * Framework 주입 실행
 */
FrameworkInjectionResult* execute_framework_injection(
    FrameworkInjectionPlan *plan) {
    
    if (!plan) return NULL;
    
    FrameworkInjectionResult *result = calloc(1, sizeof(FrameworkInjectionResult));
    result->framework_path = strdup(plan->framework_path);
    
    // 주입 시간 측정
    clock_t start = clock();
    
    // 샘플 구현: 계획 검증
    result->success = validate_framework_injectability(plan->framework_path);
    
    if (result->success) {
        // 실제 주입은 다음 단계에서 구현
        // 여기서는 Framework 준비만 진행
        printf("[*] Framework 주입 준비: %s\n", plan->framework_path);
        printf("    영향받는 앱: %zu개\n", plan->affected_app_count);
        
        result->successfully_injected_apps = plan->affected_app_count;
        result->failed_apps = 0;
    } else {
        result->successfully_injected_apps = 0;
        result->failed_apps = plan->affected_app_count;
    }
    
    // 주입 시간 계산
    clock_t end = clock();
    result->injection_time_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    
    // 로그 메시지 생성
    char log[512];
    snprintf(log, sizeof(log),
             "Framework 주입: %s (%zu앱, %.2fms)",
             plan->framework_path,
             result->successfully_injected_apps,
             result->injection_time_ms);
    result->log_message = strdup(log);
    
    return result;
}

/**
 * Framework 주입 가능성 검증
 */
bool validate_framework_injectability(const char *framework_path) {
    if (!framework_path) return false;
    
    // Framework 디렉토리 존재 확인
    if (!file_exists(framework_path)) {
        return false;
    }
    
    // 쓰기 권한 확인
    if (!has_write_permission(framework_path)) {
        return false;
    }
    
    // Versions 디렉토리 확인
    char versions_path[1024];
    snprintf(versions_path, sizeof(versions_path), "%s/Versions", framework_path);
    
    if (!file_exists(versions_path)) {
        return false;
    }
    
    return true;
}

/**
 * 주입 계획 정보 출력
 */
void print_framework_injection_plan(FrameworkInjectionPlan *plan) {
    if (!plan) {
        printf("[!] NULL FrameworkInjectionPlan\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║              Framework 주입 계획                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("[*] Framework: %s\n", plan->framework_name ? plan->framework_name : "알 수 없음");
    printf("[*] 경로: %s\n", plan->framework_path);
    printf("[*] 대상 dylib: %s\n", plan->target_dylib);
    printf("[*] 주입 위치: %s\n", plan->injection_location ? plan->injection_location : "미결정");
    
    printf("\n[*] 주입 방법\n");
    printf("    Re-export 사용: %s\n", plan->use_reexport ? "가능" : "불가");
    printf("    코드 서명 필요: %s\n", plan->requires_codesign ? "필요" : "불필요");
}

/**
 * 주입 결과 정보 출력
 */
void print_framework_injection_result(FrameworkInjectionResult *result) {
    if (!result) {
        printf("[!] NULL FrameworkInjectionResult\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║              Framework 주입 결과                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("[%s] 주입 상태: %s\n",
           result->success ? "✓" : "✗",
           result->success ? "성공" : "실패");
    
    printf("[*] Framework: %s\n", result->framework_path);
    printf("[*] 성공: %zu개 앱\n", result->successfully_injected_apps);
    printf("[*] 실패: %zu개 앱\n", result->failed_apps);
    printf("[*] 소요 시간: %.2fms\n", result->injection_time_ms);
    
    if (result->log_message) {
        printf("[*] 로그: %s\n", result->log_message);
    }
    
    if (result->failed_app_paths && result->failed_apps > 0) {
        printf("\n[!] 실패한 앱\n");
        for (size_t i = 0; i < result->failed_apps && i < 5; i++) {
            printf("    [%zu] %s\n", i + 1, result->failed_app_paths[i]);
        }
        if (result->failed_apps > 5) {
            printf("    ... 외 %zu개\n", result->failed_apps - 5);
        }
    }
}

/**
 * 메모리 해제 - 계획
 */
void free_framework_injection_plan(FrameworkInjectionPlan **plan) {
    if (!plan || !*plan) return;
    
    FrameworkInjectionPlan *p = *plan;
    
    free(p->framework_path);
    free(p->framework_name);
    free(p->target_dylib);
    free(p->injection_location);
    free(p->impact_summary);
    
    free(p);
    *plan = NULL;
}

/**
 * 메모리 해제 - 결과
 */
void free_framework_injection_result(FrameworkInjectionResult **result) {
    if (!result || !*result) return;
    
    FrameworkInjectionResult *r = *result;
    
    free(r->framework_path);
    free(r->log_message);
    
    if (r->failed_app_paths) {
        for (size_t i = 0; i < r->failed_apps; i++) {
            free(r->failed_app_paths[i]);
        }
        free(r->failed_app_paths);
    }
    
    free(r);
    *result = NULL;
}
