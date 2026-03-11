#include "binary_scanner.h"
#include "macho_parser.h"
#include "code_signing.h"
#include "hijack_detector.h"
#include "vuln_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 프라이버시 보호 디렉토리 목록 (Mojave+ macOS)
static const char* PROTECTED_DIRECTORIES[] = {
    "Library/Application Support/AddressBook",
    "Library/Calendars",
    "Pictures",
    "Library/Mail",
    "Library/Messages",
    "Library/Safari",
    "Library/Cookies",
    "Library/HomeKit",
    "Library/IdentityServices",
    "Library/Metadata/CoreSpotlight",
    "Library/PersonalizationPortrait",
    "Library/Suggestions",
    NULL  // 종료 마커
};

// 보호된 디렉토리 여부 확인
bool is_protected_directory(const char *path) {
    if (!path) return false;
    
    // 사용자 홈 디렉토리에서 상대 경로로 변환
    const char *home = getenv("HOME");
    if (!home) return false;
    
    char fullPath[4096];
    
    // 경로가 ~ 또는 $HOME으로 시작하는 경우 확장
    if (path[0] == '~') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", home, path + 1);
    } else if (strncmp(path, home, strlen(home)) == 0) {
        strncpy(fullPath, path, sizeof(fullPath) - 1);
    } else {
        return false;  // 홈 디렉토리 밖의 경로는 보호 대상 아님
    }
    
    // 검사
    for (int i = 0; PROTECTED_DIRECTORIES[i] != NULL; i++) {
        // 홈 디렉토리 기준 경로 구성
        char protectedPath[4096];
        snprintf(protectedPath, sizeof(protectedPath), "%s/%s", home, PROTECTED_DIRECTORIES[i]);
        
        // 경로가 보호 디렉토리에 포함되는지 확인
        if (strncmp(fullPath, protectedPath, strlen(protectedPath)) == 0) {
            return true;
        }
    }
    
    return false;
}

bool scan_binary(const char *path, ProcessReport *report) {
    // [FILTER 1] 보호된 디렉토리 필터
    if (is_protected_directory(path)) {
        return false;  // 프라이버시 프롬프트 방지하려 스캔 스킵
    }
    MachOParser *parser = parse_binary(path);
    if (!parser || !parser->isParsed) {
        if (parser) free_macho_parser(parser);
        return false;
    }

    bool isApple = false;
    bool hardenedRuntime = false;
    bool libValidation = false;
    bool disabledLibValidation = false;
    if (!get_signing_info(path, &isApple, &hardenedRuntime, &libValidation, &disabledLibValidation)) {
        // 서명 정보 없으면 스캔 계속 진행
    }

    // Apple 플랫폼 바이너리 및 라이브러리 검증이 정상적이면 스킵
    if (!disabledLibValidation && isApple) { 
        free_macho_parser(parser);
        return false;
    }
    // 강화된 런타임을 사용하는 경우 스킵
    if (!disabledLibValidation && hardenedRuntime) { 
        free_macho_parser(parser);
        return false;
    }
    // 라이브러리 검증 enabled 바이너리 스킵
    if (!disabledLibValidation && libValidation) { 
        free_macho_parser(parser);
        return false;
    }

    // 하이재킹 체크 (세부 유형)
    char *hijackRpathDetails = NULL;
    char *hijackWeakDetails = NULL;
    bool hijack_rpath = scan_for_hijack_rpath(path, parser, &hijackRpathDetails);
    bool hijack_weak = scan_for_hijack_weak(path, parser, &hijackWeakDetails);
    bool isHijacked = hijack_rpath || hijack_weak;

    // 취약점 체크 (세부 유형)
    char *vulnRpathDetails = NULL;
    char *vulnWeakDetails = NULL;
    bool isVulnerableRpath = scan_for_vulnerable_rpath(path, parser, &vulnRpathDetails);
    bool isVulnerableWeak = scan_for_vulnerable_weak(path, parser, &vulnWeakDetails);
    bool isVulnerable = isVulnerableRpath || isVulnerableWeak;

    // ProcessReport에 결과 저장 (세부 유형 설명 포함)
    if ((isHijacked || isVulnerable) && report) {
        if (isHijacked) {
            if (hijack_rpath) {
                process_report_add_issue(report, path, ISSUE_HIJACKED, "RPATH 다중 후보 하이재킹", hijackRpathDetails ? hijackRpathDetails : "rpath 하이재킹");
            }
            if (hijack_weak) {
                process_report_add_issue(report, path, ISSUE_HIJACKED, "Weak Dylib 하이재킹", hijackWeakDetails ? hijackWeakDetails : "weak dylib 하이재킹");
            }
        }
        if (isVulnerable) {
            if (isVulnerableRpath) {
                process_report_add_issue(report, path, ISSUE_VULNERABLE, "RPATH 취약 경로", vulnRpathDetails ? vulnRpathDetails : "rpath 취약점");
            }
            if (isVulnerableWeak) {
                process_report_add_issue(report, path, ISSUE_VULNERABLE, "Weak Dylib 취약 경로", vulnWeakDetails ? vulnWeakDetails : "weak dylib 취약점");
            }
        }

    /* free detail strings if set */
    if (hijackRpathDetails) free(hijackRpathDetails);
    if (hijackWeakDetails) free(hijackWeakDetails);
    if (vulnRpathDetails) free(vulnRpathDetails);
    if (vulnWeakDetails) free(vulnWeakDetails);
    }

    free_macho_parser(parser);
    return isHijacked || isVulnerable;
}
