/**
 * modular_injector.c
 * 
 * Type C (모듈화 체인) 주입 구현
 */

#include "modular_injector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

/* ================================================================
 * 내부 헬퍼 함수들
 * ================================================================ */

/**
 * 앱 이름 추출 (경로에서)
 */
static char* extract_app_name(const char *app_bundle_path) {
    if (!app_bundle_path) return NULL;
    
    const char *last_slash = strrchr(app_bundle_path, '/');
    const char *name_start = last_slash ? last_slash + 1 : app_bundle_path;
    
    const char *app_ext = strstr(name_start, ".app");
    if (!app_ext) return strdup(name_start);
    
    size_t len = app_ext - name_start;
    char *name = malloc(len + 1);
    strncpy(name, name_start, len);
    name[len] = '\0';
    
    return name;
}

/**
 * 진입점 타입 결정
 */

/**
 * XPCServices 디렉토리에서 서비스 찾기
 */
static EntryPoint** find_xpc_services(
    const char *app_bundle_path,
    size_t *out_count) {
    
    char xpc_dir[1024];
    snprintf(xpc_dir, sizeof(xpc_dir), "%s/Contents/XPCServices", app_bundle_path);
    
    DIR *dir = opendir(xpc_dir);
    if (!dir) {
        *out_count = 0;
        return NULL;
    }
    
    EntryPoint **services = calloc(50, sizeof(EntryPoint *));
    size_t count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) && count < 50) {
        // .xpc 번들 찾기
        if (strstr(entry->d_name, ".xpc")) {
            char binary_path[1024];
            snprintf(binary_path, sizeof(binary_path),
                     "%s/%s/Contents/MacOS/%s",
                     xpc_dir, entry->d_name, 
                     entry->d_name);
            
            // .xpc 확장자 제거
            char *xpc = strstr(binary_path, ".xpc");
            if (xpc) *xpc = '\0';
            
            if (access(binary_path, F_OK) == 0) {
                EntryPoint *ep = calloc(1, sizeof(EntryPoint));
                ep->entry_point_type = strdup("xpc_service");
                ep->binary_path = strdup(binary_path);
                ep->display_name = strdup(entry->d_name);
                ep->is_injectable = true;
                
                services[count++] = ep;
            }
        }
    }
    
    closedir(dir);
    
    *out_count = count;
    return count > 0 ? services : NULL;
}

/**
 * PlugIns 디렉토리에서 확장 찾기
 */
static EntryPoint** find_plugin_extensions(
    const char *app_bundle_path,
    size_t *out_count) {
    
    char plugins_dir[1024];
    snprintf(plugins_dir, sizeof(plugins_dir), "%s/Contents/PlugIns", app_bundle_path);
    
    DIR *dir = opendir(plugins_dir);
    if (!dir) {
        *out_count = 0;
        return NULL;
    }
    
    EntryPoint **extensions = calloc(50, sizeof(EntryPoint *));
    size_t count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) && count < 50) {
        // .appex 번들 찾기
        if (strstr(entry->d_name, ".appex")) {
            char binary_path[1024];
            snprintf(binary_path, sizeof(binary_path),
                     "%s/%s/Contents/MacOS/%s",
                     plugins_dir, entry->d_name,
                     entry->d_name);
            
            // .appex 확장자 제거
            char *appex = strstr(binary_path, ".appex");
            if (appex) *appex = '\0';
            
            if (access(binary_path, F_OK) == 0) {
                EntryPoint *ep = calloc(1, sizeof(EntryPoint));
                ep->entry_point_type = strdup("app_extension");
                ep->binary_path = strdup(binary_path);
                ep->display_name = strdup(entry->d_name);
                ep->is_injectable = true;
                
                extensions[count++] = ep;
            }
        }
    }
    
    closedir(dir);
    
    *out_count = count;
    return count > 0 ? extensions : NULL;
}

/**
 * otool -L로 RPATH 추출
 */
static char** extract_rpath_for_binary(
    const char *binary_path,
    size_t *out_count) {
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "otool -l '%s' 2>/dev/null | grep 'path ' | grep -v '(offset'", binary_path);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        *out_count = 0;
        return NULL;
    }
    
    char **rpaths = calloc(20, sizeof(char *));
    size_t count = 0;
    char line[512];
    
    while (fgets(line, sizeof(line), fp) && count < 20) {
        // "     path /some/path (offset" 형식
        char path[256];
        if (sscanf(line, "%*s path %255s", path) == 1) {
            // "(offset...)" 제거
            char *paren = strchr(path, '(');
            if (paren) *paren = '\0';
            
            rpaths[count++] = strdup(path);
        }
    }
    
    pclose(fp);
    
    *out_count = count;
    return count > 0 ? rpaths : NULL;
}

/**
 * Framework 목록 추출
 */
static char** extract_frameworks_in_app(
    const char *app_bundle_path,
    size_t *out_count) {
    
    char frameworks_dir[1024];
    snprintf(frameworks_dir, sizeof(frameworks_dir), "%s/Contents/Frameworks", app_bundle_path);
    
    DIR *dir = opendir(frameworks_dir);
    if (!dir) {
        *out_count = 0;
        return NULL;
    }
    
    char **frameworks = calloc(50, sizeof(char *));
    size_t count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) && count < 50) {
        if (strstr(entry->d_name, ".framework")) {
            char fw_path[1024];
            snprintf(fw_path, sizeof(fw_path), "%s/%s", frameworks_dir, entry->d_name);
            frameworks[count++] = strdup(fw_path);
        }
    }
    
    closedir(dir);
    
    *out_count = count;
    return count > 0 ? frameworks : NULL;
}

/* ================================================================
 * 공개 API 구현
 * ================================================================ */

/**
 * 모든 진입점 추출
 */
EntryPoint** extract_all_entry_points(
    const char *app_bundle_path,
    size_t *out_count) {
    
    if (!app_bundle_path) {
        *out_count = 0;
        return NULL;
    }
    
    EntryPoint **all_entries = calloc(100, sizeof(EntryPoint *));
    size_t total_count = 0;
    
    // 메인 진입점 추가
    char main_binary[1024];
    snprintf(main_binary, sizeof(main_binary), "%s/Contents/MacOS", app_bundle_path);
    
    DIR *main_dir = opendir(main_binary);
    if (main_dir) {
        struct dirent *entry;
        while ((entry = readdir(main_dir)) && total_count < 100) {
            // 숨김파일 제외, 디렉토리 제외
            if (entry->d_name[0] == '.' || entry->d_type == DT_DIR) continue;
            
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", main_binary, entry->d_name);
            
            if (access(full_path, X_OK) == 0) {
                EntryPoint *ep = calloc(1, sizeof(EntryPoint));
                ep->entry_point_type = strdup("main");
                ep->binary_path = strdup(full_path);
                ep->display_name = strdup(entry->d_name);
                ep->is_injectable = true;
                
                all_entries[total_count++] = ep;
            }
        }
        closedir(main_dir);
    }
    
    // XPC Services 추가
    size_t xpc_count = 0;
    EntryPoint **xpc_services = find_xpc_services(app_bundle_path, &xpc_count);
    if (xpc_services && xpc_count > 0) {
        for (size_t i = 0; i < xpc_count && total_count < 100; i++) {
            all_entries[total_count++] = xpc_services[i];
        }
        free(xpc_services);
    }
    
    // PlugIns 추가
    size_t plugin_count = 0;
    EntryPoint **plugins = find_plugin_extensions(app_bundle_path, &plugin_count);
    if (plugins && plugin_count > 0) {
        for (size_t i = 0; i < plugin_count && total_count < 100; i++) {
            all_entries[total_count++] = plugins[i];
        }
        free(plugins);
    }
    
    // 각 진입점의 RPATH 추출
    for (size_t i = 0; i < total_count; i++) {
        all_entries[i]->rpath_list = extract_rpath_for_binary(
            all_entries[i]->binary_path,
            &all_entries[i]->rpath_count);
    }
    
    *out_count = total_count;
    return total_count > 0 ? all_entries : NULL;
}

/**
 * 모듈화 의존성 트리 구성
 */
char* build_modular_dependency_tree(
    EntryPoint **entry_points,
    size_t entry_point_count) {

    if (!entry_points || entry_point_count == 0) return strdup("(의존성 없음)");
    
    char *result = malloc(2048);
    char *ptr = result;
    int remaining = 2048;
    
    int written = snprintf(ptr, remaining, 
                          "의존성 트리 (%zu개 진입점):\n", entry_point_count);
    ptr += written;
    remaining -= written;
    
    for (size_t i = 0; i < entry_point_count; i++) {
        EntryPoint *ep = entry_points[i];
        
        written = snprintf(ptr, remaining,
                          "  [%zu] %s (%s)\n",
                          i + 1, ep->display_name, ep->entry_point_type);
        ptr += written;
        remaining -= written;
        
        if (ep->rpath_count > 0) {
            written = snprintf(ptr, remaining,
                              "       RPATH: %zu개 경로\n", ep->rpath_count);
            ptr += written;
            remaining -= written;
        }
    }
    
    return result;
}

/**
 * 모듈화 주입 계획 생성
 */
ModularInjectionPlan* create_modular_injection_plan(
    const char *app_bundle_path,
    const char *target_dylib) {
    
    if (!app_bundle_path || !target_dylib) return NULL;
    
    ModularInjectionPlan *plan = calloc(1, sizeof(ModularInjectionPlan));
    
    plan->app_bundle_path = strdup(app_bundle_path);
    plan->app_name = extract_app_name(app_bundle_path);
    
    // 모든 진입점 추출
    plan->entry_points = extract_all_entry_points(
        app_bundle_path, &plan->entry_point_count);
    
    // Framework 추출
    plan->internal_frameworks = extract_frameworks_in_app(
        app_bundle_path, &plan->internal_framework_count);
    
    // 최대 의존성 깊이 계산
    plan->max_dependency_depth = 0;
    for (size_t i = 0; i < plan->entry_point_count; i++) {
        if (plan->entry_points[i]->rpath_count > plan->max_dependency_depth) {
            plan->max_dependency_depth = plan->entry_points[i]->rpath_count;
        }
    }
    
    // 복잡도 평가
    plan->complexity_assessment = assess_modular_complexity(plan);
    
    return plan;
}

/**
 * 각 진입점별 최적 주입 위치 결정
 */
char** determine_injection_points_per_entry(
    ModularInjectionPlan *plan) {
    
    if (!plan || plan->entry_point_count == 0) return NULL;
    
    char **injection_points = calloc(plan->entry_point_count, sizeof(char *));
    
    for (size_t i = 0; i < plan->entry_point_count; i++) {
        EntryPoint *ep = plan->entry_points[i];
        
        if (ep->rpath_count > 0) {
            // 첫 번째 RPATH를 주입점으로 설정
            injection_points[i] = strdup(ep->rpath_list[0]);
        } else {
            injection_points[i] = strdup("(주입 경로 없음)");
        }
    }
    
    return injection_points;
}

/**
 * 모듈화 복잡도 평가
 */
char* assess_modular_complexity(ModularInjectionPlan *plan) {
    if (!plan) return strdup("알 수 없음");
    
    char *assessment = malloc(512);
    
    // 복잡도 점수: Framework + RPATH + 진입점
    int complexity_score = 
        (plan->internal_framework_count * 10) +
        (plan->max_dependency_depth * 2) +
        (plan->entry_point_count * 5);
    
    const char *level;
    if (complexity_score > 200) {
        level = "매우 높음";
    } else if (complexity_score > 100) {
        level = "높음";
    } else if (complexity_score > 50) {
        level = "중간";
    } else {
        level = "낮음";
    }
    
    snprintf(assessment, 512,
             "%s (%zu Framework, %zu 경로 깊이, %zu 진입점, 점수: %d)",
             level,
             plan->internal_framework_count,
             plan->max_dependency_depth,
             plan->entry_point_count,
             complexity_score);
    
    return assessment;
}

/**
 * 공유 Framework 식별
 */
char** identify_shared_frameworks(
    const char *app_bundle_path,
    size_t *out_count) {
    
    if (!app_bundle_path) {
        *out_count = 0;
        return NULL;
    }
    
    // 샘플: 알려진 공유 Framework 목록
    const char *known_shared[] = {
        "Sparkle",
        "CrashReporter",
        "Fabric",
        NULL
    };
    
    char **shared = calloc(20, sizeof(char *));
    size_t count = 0;
    
    // 앱의 Framework 디렉토리에서 공유 Framework 찾기
    for (int i = 0; known_shared[i] && count < 20; i++) {
        char fw_path[1024];
        snprintf(fw_path, sizeof(fw_path),
                 "%s/Contents/Frameworks/%s.framework",
                 app_bundle_path, known_shared[i]);
        
        if (access(fw_path, F_OK) == 0) {
            shared[count++] = strdup(known_shared[i]);
        }
    }
    
    *out_count = count;
    return count > 0 ? shared : NULL;
}

/**
 * 모듈화 주입 실행
 */
ModularInjectionResult* execute_modular_injection(
    ModularInjectionPlan *plan) {
    
    if (!plan) return NULL;
    
    ModularInjectionResult *result = calloc(1, sizeof(ModularInjectionResult));
    
    result->total_entry_points = plan->entry_point_count;
    result->entry_point_results = calloc(plan->entry_point_count, sizeof(bool));
    result->per_entry_point_time_ms = calloc(plan->entry_point_count, sizeof(double));
    
    clock_t start = clock();
    
    // 각 진입점에 대해 주입 시뮬레이션
    for (size_t i = 0; i < plan->entry_point_count; i++) {
        clock_t ep_start = clock();
        
        // 주입 가능 여부 확인
        if (plan->entry_points[i]->is_injectable) {
            result->entry_point_results[i] = true;
            result->successfully_injected++;
        } else {
            result->entry_point_results[i] = false;
            result->failed_entry_points++;
        }
        
        clock_t ep_end = clock();
        result->per_entry_point_time_ms[i] = 
            (double)(ep_end - ep_start) / CLOCKS_PER_SEC * 1000;
    }
    
    clock_t end = clock();
    result->total_injection_time_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000;
    
    result->success = (result->failed_entry_points == 0);
    
    return result;
}

/**
 * 모든 진입점 검증
 */
bool validate_all_entry_points(
    ModularInjectionPlan *plan,
    ModularInjectionResult *result) {
    
    if (!plan || !result) return false;
    
    // 모든 진입점이 주입되었는지 확인
    for (size_t i = 0; i < plan->entry_point_count; i++) {
        if (!result->entry_point_results[i]) {
            return false;
        }
    }
    
    return true;
}

/**
 * 주입 계획 정보 출력
 */
void print_modular_injection_plan(ModularInjectionPlan *plan) {
    if (!plan) {
        printf("[!] NULL ModularInjectionPlan\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║              모듈화 주입 계획                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("[*] 앱: %s\n", plan->app_name);
    printf("[*] 경로: %s\n", plan->app_bundle_path);
    
    printf("\n[*] 진입점: %zu개\n", plan->entry_point_count);
    for (size_t i = 0; i < plan->entry_point_count; i++) {
        printf("    [%zu] %s (%s)\n", i + 1,
               plan->entry_points[i]->display_name,
               plan->entry_points[i]->entry_point_type);
    }
    
    printf("\n[*] Framework: %zu개 (내부)\n", plan->internal_framework_count);
    printf("[*] 공유 Framework: %zu개\n", plan->shared_framework_count);
    printf("[*] 최대 의존성 깊이: %zu\n", plan->max_dependency_depth);
    
    printf("\n[*] 복잡도: %s\n", plan->complexity_assessment);
}

/**
 * 주입 결과 정보 출력
 */
void print_modular_injection_result(ModularInjectionResult *result) {
    if (!result) {
        printf("[!] NULL ModularInjectionResult\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║              모듈화 주입 결과                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("[%s] 전체 상태: %s\n",
           result->success ? "✓" : "✗",
           result->success ? "성공" : "부분 실패");
    
    printf("[*] 진입점: %zu개\n", result->total_entry_points);
    printf("    성공: %zu개\n", result->successfully_injected);
    printf("    실패: %zu개\n", result->failed_entry_points);
    
    printf("[*] 소요 시간: %.2fms\n", result->total_injection_time_ms);
    printf("[*] 평균 진입점당: %.2fms\n",
           result->total_entry_points > 0 ?
           result->total_injection_time_ms / result->total_entry_points : 0);
}

/**
 * 진입점 정보 출력
 */
void print_entry_point(EntryPoint *ep, int indent) {
    if (!ep) return;
    
    char prefix[64] = "";
    for (int i = 0; i < indent; i++) {
        strcat(prefix, "  ");
    }
    
    printf("%s[%s] %s\n", prefix, ep->entry_point_type, ep->display_name);
    printf("%s  경로: %s\n", prefix, ep->binary_path);
    printf("%s  RPATH: %zu개\n", prefix, ep->rpath_count);
}

/**
 * 메모리 해제 - 진입점
 */
void free_entry_points(EntryPoint ***entry_points, size_t count) {
    if (!entry_points || !*entry_points) return;
    
    for (size_t i = 0; i < count; i++) {
        EntryPoint *ep = (*entry_points)[i];
        if (ep) {
            free(ep->entry_point_type);
            free(ep->binary_path);
            free(ep->display_name);
            if (ep->rpath_list) {
                for (size_t j = 0; j < ep->rpath_count; j++) {
                    free(ep->rpath_list[j]);
                }
                free(ep->rpath_list);
            }
            if (ep->framework_deps) {
                for (size_t j = 0; j < ep->framework_dep_count; j++) {
                    free(ep->framework_deps[j]);
                }
                free(ep->framework_deps);
            }
            free(ep);
        }
    }
    
    free(*entry_points);
    *entry_points = NULL;
}

/**
 * 메모리 해제 - 계획
 */
void free_modular_injection_plan(ModularInjectionPlan **plan) {
    if (!plan || !*plan) return;
    
    ModularInjectionPlan *p = *plan;
    
    free(p->app_bundle_path);
    free(p->app_name);
    free(p->injection_strategy);
    free(p->complexity_assessment);
    
    if (p->entry_points) {
        free_entry_points(&p->entry_points, p->entry_point_count);
    }
    
    if (p->internal_frameworks) {
        for (size_t i = 0; i < p->internal_framework_count; i++) {
            free(p->internal_frameworks[i]);
        }
        free(p->internal_frameworks);
    }
    
    if (p->shared_frameworks) {
        for (size_t i = 0; i < p->shared_framework_count; i++) {
            free(p->shared_frameworks[i]);
        }
        free(p->shared_frameworks);
    }
    
    free(p);
    *plan = NULL;
}

/**
 * 메모리 해제 - 결과
 */
void free_modular_injection_result(ModularInjectionResult **result) {
    if (!result || !*result) return;
    
    ModularInjectionResult *r = *result;
    
    if (r->entry_point_results) {
        free(r->entry_point_results);
    }
    
    if (r->per_entry_point_time_ms) {
        free(r->per_entry_point_time_ms);
    }
    
    if (r->failed_entry_paths) {
        for (size_t i = 0; i < r->failed_entry_points; i++) {
            free(r->failed_entry_paths[i]);
        }
        free(r->failed_entry_paths);
    }
    
    free(r);
    *result = NULL;
}
