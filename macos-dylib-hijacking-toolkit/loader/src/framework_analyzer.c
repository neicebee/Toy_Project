#include "framework_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>

/**
 * 경로에서 파일명 추출 (basename 대체)
 */
static char* get_path_basename(const char *path) {
    if (!path) return NULL;
    
    const char *ptr = path;
    const char *last_slash = NULL;
    
    // 마지막 '/' 찾기
    while (*ptr) {
        if (*ptr == '/') {
            last_slash = ptr;
        }
        ptr++;
    }
    
    if (last_slash) {
        return (char *)(last_slash + 1);
    }
    return (char *)path;
}

/**
 * .framework 번들 인지 확인
 */
static bool is_framework_bundle(const char *path) {
    if (!path) return false;
    
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    
    return (strcmp(ext, ".framework") == 0);
}

/**
 * Framework의 주요 dylib 찾기
 * 보통: FrameworkName.framework/FrameworkName 또는
 *       FrameworkName.framework/Versions/A/FrameworkName
 */
static char* find_framework_main_dylib(const char *framework_path) {
    if (!framework_path) return NULL;
    
    char *result = NULL;
    
    // Framework 이름 추출 (예: Sparkle.framework → Sparkle)
    char *name_copy = strdup(framework_path);
    char *base = get_path_basename(name_copy);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    
    char framework_name[256];
    snprintf(framework_name, sizeof(framework_name), "%s", base);
    free(name_copy);
    
    // 가능한 위치들을 시도
    char *possible_paths[] = {
        "%s/Versions/A/%s",
        "%s/Versions/Current/%s",
        "%s/%s",
        NULL
    };
    
    for (int i = 0; possible_paths[i] != NULL; i++) {
        char test_path[2048];
        snprintf(test_path, sizeof(test_path), possible_paths[i], 
                 framework_path, framework_name);
        
        struct stat st;
        if (stat(test_path, &st) == 0) {
            result = strdup(test_path);
            break;
        }
    }
    
    return result;
}

/**
 * Framework 내의 모든 dylib 열거
 */
static char** list_dylibs_in_framework_internal(
    const char *framework_path, 
    size_t *out_count) {
    
    if (!framework_path || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    char **dylibs = NULL;
    size_t count = 0;
    size_t capacity = 16;
    
    dylibs = calloc(capacity, sizeof(char *));
    
    // 재귀적으로 디렉토리 순회
    DIR *dir = opendir(framework_path);
    if (!dir) {
        *out_count = 0;
        free(dylibs);
        return NULL;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", 
                 framework_path, entry->d_name);
        
        // .dylib 파일 확인
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".dylib") == 0) {
            if (count >= capacity) {
                capacity *= 2;
                dylibs = realloc(dylibs, capacity * sizeof(char *));
            }
            dylibs[count++] = strdup(full_path);
        }
        
        // 디렉토리면 재귀 (Versions 등)
        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            size_t subcount = 0;
            char **subdylibs = list_dylibs_in_framework_internal(full_path, &subcount);
            
            for (size_t i = 0; i < subcount; i++) {
                if (count >= capacity) {
                    capacity *= 2;
                    dylibs = realloc(dylibs, capacity * sizeof(char *));
                }
                dylibs[count++] = subdylibs[i];
            }
            free(subdylibs);
        }
    }
    
    closedir(dir);
    
    *out_count = count;
    return dylibs;
}

/**
 * 앱 번들 내의 모든 Framework 찾기
 */
static char** find_frameworks_in_app(
    const char *app_bundle_path, 
    size_t *out_count) {
    
    if (!app_bundle_path || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    char **frameworks = NULL;
    size_t count = 0;
    size_t capacity = 16;
    
    frameworks = calloc(capacity, sizeof(char *));
    
    // Contents/Frameworks 디렉토리 스캔
    char frameworks_dir[2048];
    snprintf(frameworks_dir, sizeof(frameworks_dir), 
             "%s/Contents/Frameworks", app_bundle_path);
    
    DIR *dir = opendir(frameworks_dir);
    if (!dir) {
        *out_count = 0;
        free(frameworks);
        return NULL;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", 
                 frameworks_dir, entry->d_name);
        
        // .framework 확인
        if (is_framework_bundle(full_path)) {
            if (count >= capacity) {
                capacity *= 2;
                frameworks = realloc(frameworks, capacity * sizeof(char *));
            }
            frameworks[count++] = strdup(full_path);
        }
    }
    
    closedir(dir);
    
    *out_count = count;
    return frameworks;
}

/**
 * Framework의 의존성 추출 (otool -L)
 */
static char** extract_framework_dependencies_internal(
    const char *dylib_path,
    size_t *out_count) {
    
    if (!dylib_path || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    char **deps = NULL;
    size_t count = 0;
    size_t capacity = 16;
    
    deps = calloc(capacity, sizeof(char *));
    
    // otool -L을 사용하여 의존성 추출
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "otool -L '%s' 2>/dev/null", dylib_path);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        *out_count = 0;
        free(deps);
        return NULL;
    }
    
    char line[1024];
    bool first_line = true;
    
    while (fgets(line, sizeof(line), fp)) {
        // 첫 번째 줄(바이너리 이름)은 스킵
        if (first_line) {
            first_line = false;
            continue;
        }
        
        // 경로만 추출
        char *path = line;
        while (*path == ' ' || *path == '\t') path++;
        
        if (!*path || strstr(path, "Binary")) continue;
        
        // 끝의 공백/개행 제거
        char *end = strchr(path, '\t');
        if (!end) end = strchr(path, '(');
        if (!end) end = strchr(path, '\n');
        
        if (end) *end = '\0';
        
        // 경로가 유효한지 확인
        if (strlen(path) > 0 && (path[0] == '/' || strstr(path, "@"))) {
            if (count >= capacity) {
                capacity *= 2;
                deps = realloc(deps, capacity * sizeof(char *));
            }
            deps[count++] = strdup(path);
        }
    }
    
    pclose(fp);
    
    *out_count = count;
    return deps;
}

/**
 * Framework 정보 구조체 생성
 */
FrameworkInfo* create_framework_info(const char *framework_path) {
    if (!framework_path || !is_framework_bundle(framework_path)) {
        fprintf(stderr, "[ERROR] 유효하지 않은 Framework 경로: %s\n", framework_path);
        return NULL;
    }
    
    FrameworkInfo *info = calloc(1, sizeof(FrameworkInfo));
    info->path = strdup(framework_path);
    
    // Framework 이름 추출
    char *name_copy = strdup(framework_path);
    char *base = get_path_basename(name_copy);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    info->name = strdup(base);
    free(name_copy);
    
    // 주요 dylib 찾기
    char *main_dylib = find_framework_main_dylib(framework_path);
    if (main_dylib) {
        // dylib 목록에 추가
        info->dylibs = calloc(1, sizeof(char *));
        info->dylibs[0] = main_dylib;
        info->dylib_count = 1;
    }
    
    return info;
}

/**
 * 공개 API: Framework 분석
 */
FrameworkInfo* analyze_framework(const char *framework_path) {
    FrameworkInfo *info = create_framework_info(framework_path);
    if (!info) return NULL;
    
    printf("[*] Framework 분석: %s\n", info->name);
    printf("    경로: %s\n", framework_path);
    
    // 모든 dylib 열거
    size_t dylib_count = 0;
    char **all_dylibs = list_dylibs_in_framework_internal(framework_path, &dylib_count);
    
    // 기존 dylib 추가
    if (all_dylibs) {
        if (info->dylib_count > 0) {
            info->dylibs = realloc(info->dylibs, (info->dylib_count + dylib_count) * sizeof(char *));
            for (size_t i = 0; i < dylib_count; i++) {
                info->dylibs[info->dylib_count + i] = all_dylibs[i];
            }
            info->dylib_count += dylib_count;
        } else {
            info->dylibs = all_dylibs;
            info->dylib_count = dylib_count;
        }
    }
    
    printf("    dylib 수: %zu개\n", info->dylib_count);
    
    // 의존성 추출 (주요 dylib만)
    if (info->dylib_count > 0) {
        size_t dep_count = 0;
        char **deps = extract_framework_dependencies_internal(info->dylibs[0], &dep_count);
        info->dependencies = deps;
        info->dep_count = dep_count;
        printf("    의존성: %zu개\n", dep_count);
    }
    
    return info;
}

/**
 * 공개 API: 앱 내 Framework 분석
 */
FrameworkInfo** analyze_frameworks_in_app(
    const char *app_bundle_path,
    size_t *out_count) {
    
    if (!app_bundle_path || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    printf("[*] 앱 번들 분석: %s\n", app_bundle_path);
    
    // 앱 내 모든 Framework 찾기
    size_t fw_count = 0;
    char **framework_paths = find_frameworks_in_app(app_bundle_path, &fw_count);
    
    if (fw_count == 0) {
        printf("    Framework 없음\n");
        *out_count = 0;
        return NULL;
    }
    
    printf("    발견된 Framework: %zu개\n", fw_count);
    
    // 각 Framework 분석
    FrameworkInfo **infos = calloc(fw_count, sizeof(FrameworkInfo *));
    
    for (size_t i = 0; i < fw_count; i++) {
        infos[i] = analyze_framework(framework_paths[i]);
        free(framework_paths[i]);
        
        if (infos[i]) {
            printf("      [%zu] %s\n", i + 1, infos[i]->name);
        }
    }
    
    free(framework_paths);
    
    *out_count = fw_count;
    return infos;
}

/**
 * 공개 API: Framework 내 dylib 열거
 */
char** list_dylibs_in_framework(const char *framework_path, size_t *out_count) {
    return list_dylibs_in_framework_internal(framework_path, out_count);
}

/**
 * 공개 API: Framework 의존성 추출
 */
char** extract_framework_dependencies(const char *framework_path, size_t *out_count) {
    char *main_dylib = find_framework_main_dylib(framework_path);
    if (!main_dylib) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    char **deps = extract_framework_dependencies_internal(main_dylib, out_count);
    free(main_dylib);
    return deps;
}

/**
 * 공개 API: Framework 정보 해제
 */
void free_framework_info(FrameworkInfo *info) {
    if (!info) return;
    
    free(info->path);
    free(info->name);
    
    for (size_t i = 0; i < info->dylib_count; i++) {
        free(info->dylibs[i]);
    }
    free(info->dylibs);
    
    for (size_t i = 0; i < info->dep_count; i++) {
        free(info->dependencies[i]);
    }
    free(info->dependencies);
    
    for (size_t i = 0; i < info->internal_fw_count; i++) {
        free(info->internal_frameworks[i]);
    }
    free(info->internal_frameworks);
    
    for (size_t i = 0; i < info->rpath_count; i++) {
        free(info->rpath_chain[i]);
    }
    free(info->rpath_chain);
    
    free(info);
}

/**
 * 공개 API: 모든 Framework 정보 해제
 */
void free_all_framework_infos(FrameworkInfo **infos, size_t count) {
    if (!infos) return;
    
    for (size_t i = 0; i < count; i++) {
        free_framework_info(infos[i]);
    }
    free(infos);
}

/**
 * 공개 API: Framework 정보 출력 (디버깅용)
 */
void print_framework_info(FrameworkInfo *info) {
    if (!info) return;
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Framework: %s\n", info->name);
    printf("경로: %s\n", info->path);
    
    printf("\n[Dylibs (%zu개)]\n", info->dylib_count);
    for (size_t i = 0; i < info->dylib_count; i++) {
        printf("  [%zu] %s\n", i + 1, info->dylibs[i]);
    }
    
    printf("\n[의존성 (%zu개)]\n", info->dep_count);
    for (size_t i = 0; i < info->dep_count; i++) {
        printf("  [%zu] %s\n", i + 1, info->dependencies[i]);
    }
    
    printf("\n[내부 Framework (%zu개)]\n", info->internal_fw_count);
    for (size_t i = 0; i < info->internal_fw_count; i++) {
        printf("  [%zu] %s\n", i + 1, info->internal_frameworks[i]);
    }
    
    printf("\n[RPATH 체인 (%zu개)]\n", info->rpath_count);
    for (size_t i = 0; i < info->rpath_count; i++) {
        printf("  [%zu] %s\n", i + 1, info->rpath_chain[i]);
    }
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}
