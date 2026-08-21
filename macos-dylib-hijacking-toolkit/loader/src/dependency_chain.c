/**
 * dependency_chain.c
 * 
 * 의존성 그래프 분석 구현
 */

#include "dependency_chain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ================================================================
 * 내부 헬퍼 함수들
 * ================================================================ */

/**
 * dylib 이름 정규화 (예: "@rpath/libX" → "libX")
 */
static char* normalize_dylib_name(const char *spec) {
    if (!spec) return NULL;
    
    char *name = strdup(spec);
    
    // @rpath, @loader_path, @executable_path 제거
    if (strstr(name, "@rpath/")) {
        char *ptr = strchr(name, '/');
        if (ptr) {
            char *tmp = strdup(ptr + 1);
            free(name);
            name = tmp;
        }
    } else if (strstr(name, "@loader_path/")) {
        char *ptr = strchr(name, '/');
        if (ptr) {
            char *tmp = strdup(ptr + 1);
            free(name);
            name = tmp;
        }
    } else if (strstr(name, "@executable_path/")) {
        char *ptr = strchr(name, '/');
        if (ptr) {
            char *tmp = strdup(ptr + 1);
            free(name);
            name = tmp;
        }
    }
    
    return name;
}

/**
 * dylib이 시스템 라이브러리인지 확인
 */
static bool is_system_library(const char *path) {
    if (!path) return false;
    
    // /usr/lib, /System/Library 등은 시스템 라이브러리
    return (strstr(path, "/usr/lib") != NULL ||
            strstr(path, "/System/Library") != NULL ||
            strstr(path, "/opt/homebrew") != NULL);
}

/**
 * dylib이 Framework인지 확인
 */
static bool is_framework_dylib(const char *path) {
    if (!path) return false;
    return strstr(path, ".framework") != NULL;
}

/**
 * 의존성 노드 생성
 */
static DependencyNode* create_dependency_node(const char *dylib_spec) {
    if (!dylib_spec) return NULL;
    
    DependencyNode *node = calloc(1, sizeof(DependencyNode));
    
    node->dylib_name = normalize_dylib_name(dylib_spec);
    node->load_command_spec = strdup(dylib_spec);
    
    // 기본값 설정
    node->visit_state = 0;
    node->graph_depth = 0;
    node->is_system_lib = is_system_library(dylib_spec);
    node->is_framework = is_framework_dylib(dylib_spec);
    node->is_injectable = !node->is_system_lib;  // 시스템 라이브러리는 주입 불가
    node->injection_priority = node->is_injectable ? 1 : 0;
    
    // 파일 수정 시간
    struct stat st;
    if (dylib_spec && stat(dylib_spec, &st) == 0) {
        node->last_modified = st.st_mtime;
    }
    
    return node;
}

/**
 * otool -L을 사용해서 dylib의 의존성 추출
 */
static char** extract_dependencies(
    const char *binary_path,
    size_t *out_count) {
    
    if (!binary_path || !out_count) {
        *out_count = 0;
        return NULL;
    }
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "otool -L '%s' 2>/dev/null", binary_path);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        *out_count = 0;
        return NULL;
    }
    
    char **deps = calloc(100, sizeof(char *));
    size_t count = 0;
    char line[1024];
    bool first_line = true;
    
    while (fgets(line, sizeof(line), fp)) {
        // 첫 번째 줄은 바이너리 자신이므로 스킵
        if (first_line) {
            first_line = false;
            continue;
        }
        
        // 공백 제거
        char *trimmed = line;
        while (*trimmed && (*trimmed == ' ' || *trimmed == '\t')) trimmed++;
        
        // 빈 줄 스킵
        if (!*trimmed || *trimmed == '\n') continue;
        
        // 경로 추출 (첫 번째 토큰)
        char path[512];
        if (sscanf(trimmed, "%511s", path) == 1) {
            if (count < 100) {
                deps[count++] = strdup(path);
            }
        }
    }
    
    pclose(fp);
    
    *out_count = count;
    return deps;
}

/**
 * RPATH 목록 추출
 */
static char** extract_rpath_list(
    const char *binary_path,
    size_t *out_count) {
    
    if (!binary_path || !out_count) {
        *out_count = 0;
        return NULL;
    }
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "otool -l '%s' 2>/dev/null | grep -A2 'Load command'", binary_path);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        *out_count = 0;
        return NULL;
    }
    
    char **rpaths = calloc(50, sizeof(char *));
    size_t count = 0;
    char line[1024];
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "path ") && strstr(line, "(offset")) {
            // 형식: "     path /path/to (offset 12)"
            char path[512];
            if (sscanf(line, "%*s path %511s", path) == 1) {
                // "(offset...)" 제거
                char *paren = strchr(path, '(');
                if (paren) *paren = '\0';
                
                if (count < 50) {
                    rpaths[count++] = strdup(path);
                }
            }
        }
    }
    
    pclose(fp);
    
    *out_count = count;
    return rpaths;
}

/**
 * DFS를 사용한 순환 참조 감지
 */
static bool detect_cycle_dfs(DependencyNode *node, DependencyGraph *graph) {
    if (!node) return false;
    
    if (node->visit_state == 1) {
        // 현재 방문 중 → 순환 참조 발견
        return true;
    }
    
    if (node->visit_state == 2) {
        // 이미 완료 → 이미 확인됨
        return false;
    }
    
    // 방문 중으로 표시
    node->visit_state = 1;
    
    // 모든 의존성 확인
    for (size_t i = 0; i < node->dep_count; i++) {
        if (detect_cycle_dfs(node->dependencies[i], graph)) {
            return true;
        }
    }
    
    // 완료로 표시
    node->visit_state = 2;
    return false;
}

/* ================================================================
 * 공개 API 구현
 * ================================================================ */

/**
 * 의존성 그래프 생성
 */
DependencyGraph* build_dependency_graph(
    const char *binary_path,
    const char *app_bundle_path) {
    
    if (!binary_path) return NULL;
    
    DependencyGraph *graph = calloc(1, sizeof(DependencyGraph));
    
    graph->binary_path = strdup(binary_path);
    if (app_bundle_path) {
        graph->app_bundle_path = strdup(app_bundle_path);
    }
    
    // 의존성 추출
    size_t dep_count = 0;
    char **deps = extract_dependencies(binary_path, &dep_count);
    
    if (!deps || dep_count == 0) {
        free_dependency_graph(&graph);
        return NULL;
    }
    
    // 노드 생성
    graph->nodes = calloc(dep_count, sizeof(DependencyNode *));
    graph->node_count = dep_count;
    
    for (size_t i = 0; i < dep_count; i++) {
        graph->nodes[i] = create_dependency_node(deps[i]);
        free(deps[i]);
    }
    free(deps);
    
    // RPATH 추출
    graph->rpath_list = extract_rpath_list(binary_path, &graph->rpath_count);
    
    return graph;
}

/**
 * 순환 참조 감지
 */
bool detect_circular_dependency(DependencyGraph *graph) {
    if (!graph || graph->node_count == 0) return false;
    
    // 각 노드에서 DFS 시작
    for (size_t i = 0; i < graph->node_count; i++) {
        // visit_state 초기화
        for (size_t j = 0; j < graph->node_count; j++) {
            graph->nodes[j]->visit_state = 0;
        }
        
        if (detect_cycle_dfs(graph->nodes[i], graph)) {
            graph->has_circular_dependency = true;
            return true;
        }
    }
    
    graph->has_circular_dependency = false;
    return false;
}

/**
 * 최적 주입 경로 찾기
 */
char* find_best_injection_path(
    DependencyGraph *graph,
    const char *target_dylib) {
    
    if (!graph || !target_dylib) return NULL;
    
    char *best_path = NULL;
    int best_priority = -1;
    
    // 그래프의 모든 노드 중에서 최적 경로 찾기
    for (size_t i = 0; i < graph->node_count; i++) {
        DependencyNode *node = graph->nodes[i];
        
        if (!node || !node->is_injectable) continue;
        
        // dylib 이름 일치 확인
        if (strcmp(node->dylib_name, target_dylib) != 0) continue;
        
        // 우선도 비교
        if (node->injection_priority > best_priority) {
            best_priority = node->injection_priority;
            best_path = node->resolved_path ? node->resolved_path : node->load_command_spec;
        }
    }
    
    return best_path ? strdup(best_path) : NULL;
}

/**
 * 모든 주입 가능 경로 찾기
 */
char** find_all_injectable_paths(
    DependencyGraph *graph,
    const char *target_dylib,
    size_t *out_count) {
    
    if (!graph || !target_dylib || !out_count) {
        *out_count = 0;
        return NULL;
    }
    
    char **paths = calloc(graph->node_count, sizeof(char *));
    size_t count = 0;
    
    for (size_t i = 0; i < graph->node_count; i++) {
        DependencyNode *node = graph->nodes[i];
        
        if (!node || !node->is_injectable) continue;
        if (strcmp(node->dylib_name, target_dylib) != 0) continue;
        
        if (count < graph->node_count) {
            char *path = node->resolved_path ? node->resolved_path : node->load_command_spec;
            paths[count++] = strdup(path);
        }
    }
    
    *out_count = count;
    return count > 0 ? paths : NULL;
}

/**
 * 의존성 체인 길이 계산
 */
size_t calculate_dependency_chain_length(DependencyGraph *graph) {
    if (!graph || graph->node_count == 0) return 0;
    
    int max_depth = 0;

    for (size_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->graph_depth > max_depth) {
            max_depth = graph->nodes[i]->graph_depth;
        }
    }

    return (size_t)(max_depth + 1);  // 깊이 + 1 = 체인 길이
}

/**
 * 특정 dylib이 영향받는 범위 계산
 */
char** get_affected_dependents(
    DependencyGraph *graph,
    const char *dylib_name,
    size_t *out_count) {
    
    if (!graph || !dylib_name || !out_count) {
        *out_count = 0;
        return NULL;
    }
    
    char **affected = calloc(graph->node_count, sizeof(char *));
    size_t count = 0;
    
    // 대상 노드 찾기
    DependencyNode *target = NULL;
    for (size_t i = 0; i < graph->node_count; i++) {
        if (strcmp(graph->nodes[i]->dylib_name, dylib_name) == 0) {
            target = graph->nodes[i];
            break;
        }
    }
    
    if (!target || target->dependent_count == 0) {
        *out_count = 0;
        return NULL;
    }
    
    // 의존하는 dylib들 반환
    for (size_t i = 0; i < target->dependent_count; i++) {
        if (count < graph->node_count) {
            affected[count++] = strdup(target->dependents[i]->dylib_name);
        }
    }
    
    *out_count = count;
    return count > 0 ? affected : NULL;
}

/**
 * 그래프 정보 출력
 */
void print_dependency_graph(DependencyGraph *graph) {
    if (!graph) {
        printf("[!] NULL DependencyGraph\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║              의존성 그래프 분석 결과                      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("[*] 바이너리: %s\n", graph->binary_path);
    if (graph->app_bundle_path) {
        printf("[*] 앱 번들: %s\n", graph->app_bundle_path);
    }
    
    printf("\n[*] 그래프 정보\n");
    printf("    노드 수: %zu개\n", graph->node_count);
    printf("    RPATH 수: %zu개\n", graph->rpath_count);
    printf("    순환 참조: %s\n", graph->has_circular_dependency ? "있음 ⚠" : "없음 ✓");
    
    printf("\n[*] 의존성 체인 길이: %zu\n", calculate_dependency_chain_length(graph));
    
    printf("\n[*] 노드 목록\n");
    for (size_t i = 0; i < graph->node_count && i < 10; i++) {
        printf("    [%zu] %s\n", i + 1, graph->nodes[i]->dylib_name);
        if (graph->nodes[i]->is_system_lib) printf("        (시스템 라이브러리)\n");
        if (graph->nodes[i]->is_framework) printf("        (Framework)\n");
        if (!graph->nodes[i]->is_injectable) printf("        (주입 불가)\n");
    }
    
    if (graph->node_count > 10) {
        printf("    ... 외 %zu개\n", graph->node_count - 10);
    }
}

/**
 * 노드 상세 정보 출력
 */
void print_dependency_node(DependencyNode *node, int indent) {
    if (!node) return;
    
    char prefix[64] = "";
    for (int i = 0; i < indent; i++) {
        strcat(prefix, "  ");
    }
    
    printf("%s[%s]\n", prefix, node->dylib_name);
    printf("%s  경로: %s\n", prefix, node->resolved_path ? node->resolved_path : node->load_command_spec);
    printf("%s  깊이: %d, 우선도: %d\n", prefix, node->graph_depth, node->injection_priority);
    
    if (node->is_system_lib) printf("%s  [시스템]\n", prefix);
    if (node->is_framework) printf("%s  [Framework]\n", prefix);
    if (!node->is_injectable) printf("%s  [주입 불가]\n", prefix);
}

/**
 * 메모리 해제
 */
void free_dependency_graph(DependencyGraph **graph) {
    if (!graph || !*graph) return;
    
    DependencyGraph *g = *graph;
    
    if (g->nodes) {
        for (size_t i = 0; i < g->node_count; i++) {
            free_dependency_node(g->nodes[i]);
        }
        free(g->nodes);
    }
    
    if (g->rpath_list) {
        for (size_t i = 0; i < g->rpath_count; i++) {
            free(g->rpath_list[i]);
        }
        free(g->rpath_list);
    }
    
    if (g->cycle_nodes) {
        for (size_t i = 0; i < g->cycle_count; i++) {
            free(g->cycle_nodes[i]);
        }
        free(g->cycle_nodes);
    }
    
    free(g->binary_path);
    free(g->app_bundle_path);
    free(g);
    
    *graph = NULL;
}

/**
 * 단일 노드 메모리 해제
 */
void free_dependency_node(DependencyNode *node) {
    if (!node) return;
    
    free(node->dylib_name);
    free(node->resolved_path);
    free(node->load_command_spec);
    
    if (node->dependencies) {
        free(node->dependencies);
    }
    
    if (node->dependents) {
        free(node->dependents);
    }
    
    free(node);
}
