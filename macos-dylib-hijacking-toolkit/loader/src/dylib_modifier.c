#include "dylib_modifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * 시스템 명령어 실행 (shell command)
 */
static int run_command(const char *cmd) {
    int ret = system(cmd);
    return WIFEXITED(ret) ? WEXITSTATUS(ret) : -1;
}

/**
 * 문자열 앞뒤의 공백 제거
 */
static char* trim_whitespace(char *str) {
    if (!str) return str;
    
    // 앞의 공백 제거
    while (*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) {
        str++;
    }
    
    // 뒤의 공백 제거
    char *end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    return str;
}

/**
 * dylib 복사
 */
bool copy_dylib(const char *source_dylib, const char *target_path) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cp -f '%s' '%s'", source_dylib, target_path);
    
    int ret = run_command(cmd);
    if (ret != 0) {
        fprintf(stderr, "[ERROR] dylib 복사 실패: %s → %s\n", source_dylib, target_path);
        return false;
    }
    
    printf("[✓] dylib 복사 완료: %s\n", target_path);
    
    // 권한 확보 (install_name_tool이 수정 가능하도록)
    snprintf(cmd, sizeof(cmd), "chmod u+w '%s'", target_path);
    run_command(cmd);
    
    return true;
}

/**
 * install_name 변경
 * 
 * install_name은 바이너리가 이 dylib을 로드할 때 사용할 경로입니다.
 * 예를 들어, 우리가 /Applications/App/Frameworks/libswiftCore.dylib에
 * 우리의 dylib을 배치했다면, install_name을 해당 경로로 설정해야 합니다.
 */
bool change_install_name(const char *dylib_path, const char *new_install_name) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "install_name_tool -id '%s' '%s'", new_install_name, dylib_path);
    
    int ret = run_command(cmd);
    if (ret != 0) {
        fprintf(stderr, "[ERROR] install_name 변경 실패: %s\n", dylib_path);
        return false;
    }
    
    printf("[✓] install_name 변경: %s (%s)\n", new_install_name, dylib_path);
    return true;
}

/**
 * install_name 읽기
 */
char* get_dylib_install_name(const char *dylib_path) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "otool -D '%s' 2>/dev/null | tail -1", dylib_path);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "[ERROR] install_name 읽기 실패\n");
        return NULL;
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), fp)) {
        // 끝에 개행 제거
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        
        pclose(fp);
        return strdup(buffer);
    }
    
    pclose(fp);
    return NULL;
}

/**
 * dylib 또는 바이너리의 의존성을 변경합니다.
 * install_name_tool -change를 사용하여 특정 의존성을 새로운 경로로 변경합니다.
 * 
 * 동작:
 * install_name_tool -change <old_dependency> <new_dependency> <target_file>
 * 
 * 예시:
 * install_name_tool -change /usr/lib/libsystem.dylib /Custom/Path/libsystem.dylib binary
 * → binary가 libsystem.dylib을 찾을 때 /Custom/Path/libsystem.dylib을 대신 로드
 */
bool patch_dependency(const char *target_file, 
                     const char *old_dependency, 
                     const char *new_dependency) {
    if (!target_file || !old_dependency || !new_dependency) {
        fprintf(stderr, "[ERROR] patch_dependency: 유효하지 않은 파라미터\n");
        return false;
    }
    
    // 입력값 정제 (공백 제거)
    char old_dep_clean[1024], new_dep_clean[1024];
    strncpy(old_dep_clean, old_dependency, sizeof(old_dep_clean)-1);
    strncpy(new_dep_clean, new_dependency, sizeof(new_dep_clean)-1);
    old_dep_clean[sizeof(old_dep_clean)-1] = '\0';
    new_dep_clean[sizeof(new_dep_clean)-1] = '\0';
    
    char *old_trimmed = trim_whitespace(old_dep_clean);
    char *new_trimmed = trim_whitespace(new_dep_clean);
    
    printf("[*] 의존성 변경 시작\n");
    printf("   대상 파일: %s\n", target_file);
    printf("   변경 전: '%s' (길이: %zu)\n", old_trimmed, strlen(old_trimmed));
    printf("   변경 후: '%s' (길이: %zu)\n", new_trimmed, strlen(new_trimmed));
    
    // 1단계: 파일 정보/권한 확인
    struct stat st;
    if (stat(target_file, &st) != 0) {
        fprintf(stderr, "[ERROR] 파일을 찾을 수 없음: %s\n", target_file);
        return false;
    }
    
    printf("   파일 권한: %o (쓰기 가능: %s)\n", 
           st.st_mode & 0777,
           (st.st_mode & S_IWUSR) ? "yes" : "no");
    
    // 2단계: 파일 타입 확인 (Universal Binary 여부)
    printf("\n[*] 파일 타입 확인:\n");
    char file_cmd[1024];
    snprintf(file_cmd, sizeof(file_cmd), "file '%s'", target_file);
    system(file_cmd);
    
    // 3단계: 현재 의존성 확인
    printf("\n[*] 현재 의존성 목록:\n");
    char check_cmd[1024];
    snprintf(check_cmd, sizeof(check_cmd), "otool -L '%s'", target_file);
    system(check_cmd);
    
    // 4단계: install_name_tool 명령 상세 로깅
    printf("\n[*] install_name_tool 실행 중...\n");
    printf("   명령: install_name_tool -change '%s' '%s' '%s'\n", 
           old_dependency, new_dependency, target_file);
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), 
             "install_name_tool -change '%s' '%s' '%s' 2>&1; echo \"반환값: $?\"",
             old_dependency, new_dependency, target_file);
    
    printf("   [실행]\n");
    system(cmd);
    
    // 5단계: 변경 후 의존성 확인
    printf("\n[*] 변경 후 의존성 확인:\n");
    system(check_cmd);
    
    // 6단계: 변경이 적용되었는지 검증
    printf("\n[*] 변경 검증:\n");
    char verify_cmd[2048];
    snprintf(verify_cmd, sizeof(verify_cmd), 
             "otool -L '%s' | grep -E \"(^|\\s)%s\" | head -1",
             target_file, new_dependency);
    
    FILE *fp = popen(verify_cmd, "r");
    if (fp) {
        char buf[512];
        if (fgets(buf, sizeof(buf), fp) != NULL && strlen(buf) > 0) {
            printf("[✓] 검증 성공! 변경된 의존성: %s", buf);
        } else {
            printf("[WARNING] 검증 실패! 의존성이 변경되지 않았습니다\n");
            printf("   가능한 원인:\n");
            printf("   1. Universal Binary의 모든 슬라이스가 업데이트되지 않음\n");
            printf("   2. 파일이 SIP으로 보호되거나 불변\n");
            printf("   3. 바이너리 형식 호환성 문제\n");
        }
        pclose(fp);
    }
    
    // 7단계: 코드 서명 재설정
    printf("\n[*] 코드 서명 재설정 중...\n");
    char codesign_cmd[1024];
    snprintf(codesign_cmd, sizeof(codesign_cmd), 
             "codesign -f -s - '%s' 2>&1", target_file);
    
    int sign_ret = system(codesign_cmd);
    if (sign_ret == 0) {
        printf("[✓] 코드 서명 재설정 완료\n");
    } else {
        printf("[WARNING] 코드 서명 재설정 실패\n");
        printf("    SIP 보호 파일일 수 있습니다\n");
    }
    
    return true;
}

/**
 * dylib의 의존성(dependencies)을 확인합니다.
 * otool -L을 사용하여 현재 의존성 목록을 출력합니다.
 */
bool check_dylib_dependencies(const char *dylib_path) {
    if (!dylib_path) {
        fprintf(stderr, "[ERROR] check_dylib_dependencies: 유효하지 않은 경로\n");
        return false;
    }
    
    printf("[*] dylib 의존성 확인: %s\n", dylib_path);
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "otool -L '%s' 2>/dev/null | grep -v Binary", dylib_path);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "[WARNING] dylib 의존성 확인 실패\n");
        return false;
    }
    
    char line[1024];
    int dep_count = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) > 2) {
            printf("   %s", line);
            dep_count++;
        }
    }
    pclose(fp);
    
    if (dep_count == 0) {
        printf("   (의존성 없음)\n");
    }
    
    return true;
}

/**
 * dylib에서 특정 라이브러리 이름을 가진 의존성을 검색합니다.
 * 
 * 동작:
 * 1. otool -L을 실행하여 모든 의존성 획득
 * 2. 라이브러리 이름과 일치하는 의존성 검색
 * 3. 찾으면 전체 경로 반환 (예: /usr/lib/libsystem.dylib)
 */
bool find_dependency_by_name(const char *dylib_path, 
                            const char *lib_name, 
                            char **out_found_path) {
    if (!dylib_path || !lib_name || !out_found_path) {
        fprintf(stderr, "[ERROR] find_dependency_by_name: 유효하지 않은 파라미터\n");
        return false;
    }
    
    printf("[*] 의존성 검색: '%s'에서 '%s' 찾기\n", dylib_path, lib_name);
    
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "otool -L '%s' 2>/dev/null", dylib_path);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "[WARNING] 의존성 검색 실패\n");
        return false;
    }
    
    char line[1024];
    bool found = false;
    
    while (fgets(line, sizeof(line), fp)) {
        // 경로 부분만 추출 (보통 탭 또는 공백으로 시작)
        char *path_start = line;
        while (*path_start == ' ' || *path_start == '\t') {
            path_start++;
        }
        
        // 빈 줄이나 Binary 라인 스킵
        if (strlen(path_start) < 2 || strstr(path_start, "Binary")) {
            continue;
        }
        
        // 라이브러리 이름 매칭 (경로의 마지막 부분)
        if (strstr(path_start, lib_name) != NULL) {
            // 끝의 개행과 버전 정보 제거
            char *end = strchr(path_start, '\t');
            if (!end) {
                end = strchr(path_start, '(');
            }
            if (!end) {
                end = strchr(path_start, '\n');
            }
            
            if (end) {
                size_t len = end - path_start;
                *out_found_path = malloc(len + 1);
                strncpy(*out_found_path, path_start, len);
                (*out_found_path)[len] = '\0';
            } else {
                *out_found_path = strdup(path_start);
                // 끝의 개행 제거
                size_t len = strlen(*out_found_path);
                if (len > 0 && (*out_found_path)[len-1] == '\n') {
                    (*out_found_path)[len-1] = '\0';
                }
            }
            
            // 공백 정제
            char *trimmed = trim_whitespace(*out_found_path);
            if (trimmed != *out_found_path) {
                char *new_ptr = strdup(trimmed);
                free(*out_found_path);
                *out_found_path = new_ptr;
            } else {
                // trim_whitespace가 제자리에서 처리하므로 다시 할당
                char *new_ptr = strdup(*out_found_path);
                free(*out_found_path);
                *out_found_path = new_ptr;
            }
            
            printf("   ✓ 발견: '%s' (길이: %zu)\n", *out_found_path, strlen(*out_found_path));
            found = true;
            break;
        }
    }
    
    pclose(fp);
    
    if (!found) {
        printf("   ✗ '%s'를 찾을 수 없음\n", lib_name);
        return false;
    }
    
    return true;
}

/**
 * dylib에서 특정 의존성을 찾아 자동으로 패치합니다.
 * 
 * 동작 흐름:
 * 1. 의존성 검색 (find_dependency_by_name)
 * 2. 찾으면 패치 (patch_dependency)
 * 3. 없으면 스킵
 */
int auto_patch_dependency_if_found(const char *dylib_path, 
                                   const char *old_lib_name, 
                                   const char *new_dependency) {
    if (!dylib_path || !old_lib_name || !new_dependency) {
        fprintf(stderr, "[ERROR] auto_patch_dependency_if_found: 유효하지 않은 파라미터\n");
        return -1;
    }
    
    char *found_path = NULL;
    
    // 1단계: 의존성 찾기
    if (!find_dependency_by_name(dylib_path, old_lib_name, &found_path)) {
        // 의존성을 찾지 못했음 (에러가 아님)
        return 0;  // 0 = 의존성 없음
    }
    
    // 2단계: 찾은 의존성을 새로운 경로로 패치
    printf("\n[*] 의존성 자동 패치 진행\n");
    
    bool patch_success = patch_dependency(dylib_path, found_path, new_dependency);
    
    free(found_path);
    
    return patch_success ? 1 : -1;  // 1 = 성공, -1 = 패치 실패
}