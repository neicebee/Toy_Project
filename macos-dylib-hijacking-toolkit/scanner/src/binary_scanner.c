#include "binary_scanner.h"
#include "macho_parser.h"
#include "code_signing.h"
#include "hijack_detector.h"
#include "vuln_scanner.h"
#include "scan_report.h"      // ← process_scanner.h 제거, scan_report.h 사용
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    NULL
};

// 보호된 디렉토리 여부 확인
bool is_protected_directory(const char *path) {
    if (!path) return false;
    
    const char *home = getenv("HOME");
    if (!home) return false;
    
    char fullPath[4096];
    
    if (path[0] == '~') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", home, path + 1);
    } else if (strncmp(path, home, strlen(home)) == 0) {
        strncpy(fullPath, path, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';
    } else {
        return false;
    }
    
    for (int i = 0; PROTECTED_DIRECTORIES[i] != NULL; i++) {
        char protectedPath[4096];
        snprintf(protectedPath, sizeof(protectedPath), "%s/%s", home, PROTECTED_DIRECTORIES[i]);
        
        size_t protLen = strlen(protectedPath);
        
        // ─── [수정 반영] 접두사 오탐 방지를 위한 경계 검사 추가 ───
        if (strncmp(fullPath, protectedPath, protLen) == 0) {
            // 일치하는 길이 직후가 문자열의 끝('\0')이거나 디렉터리 구분자('/')인지 확인
            if (fullPath[protLen] == '\0' || fullPath[protLen] == '/') {
                return true;
            }
        }
    }
    
    return false;
}

/* ──────────────────────────────────────────────
 * 공개 API: 단일 바이너리 스캔 (경로 기반 + 프로세스 정보)
 * ────────────────────────────────────────────── */
#define BDBG(fmt, ...) if (verbose) printf("[DEBUG-BIN] " fmt "\n", ##__VA_ARGS__)
void scan_binary(const char *path,
                 ScanReport *report,
                 bool verbose,
                 pid_t pid,
                 const char *processPath,
                 const char *processName) {
    if (!path || !report) return;

    // [FILTER 1] 보호된 디렉토리 필터
    if (is_protected_directory(path)) {
        if (verbose) printf("[V] 보호된 디렉토리 건너뜀: %s\n", path);
        return;
    }

    if (verbose) printf("[V] 스캔 시작: %s\n", path);

    MachOParser *parser = parse_binary(path);
    if (!parser || !parser->isParsed) {
        if (verbose) printf("[V] Mach-O 파싱 실패 또는 파서 없음: %s\n", path);
        if (parser) free_macho_parser(parser);
        return;
    }

    // ─── 코드 서명/엔타이틀먼트 정보 획득 (경로 기반) ───
    SigningInfo info = {0};
    bool hasSigningInfo = get_signing_info_for_path(path, &info);

    // Apple 플랫폼 바이너리 + 라이브러리 검증 정상 → 스킵
    if (hasSigningInfo && info.isApple && !info.disabledLibValidation) {
        if (verbose) printf("[V] Apple 서명 + Library Validation → 스킵: %s\n", path);
        free_macho_parser(parser);
        return;
    }
    // Hardened Runtime + Library Validation 정상 → 스킵
    if (hasSigningInfo && info.hasHardenedRuntime && info.hasLibraryValidation && !info.disabledLibValidation) {
        if (verbose) printf("[V] Hardened Runtime + Library Validation → 스킵: %s\n", path);
        free_macho_parser(parser);
        return;
    }
    // Library Validation만 enabled인 경우도 스킵
    if (hasSigningInfo && info.hasLibraryValidation && !info.disabledLibValidation) {
        if (verbose) printf("[V] Library Validation enabled → 스킵: %s\n", path);
        free_macho_parser(parser);
        return;
    }

    // ─── 하이재킹 체크 ───
    char *hijackRpathDetails = NULL;
    char *hijackWeakDetails  = NULL;
    bool hijack_rpath = scan_for_hijack_rpath(path, parser, &hijackRpathDetails, verbose);
    bool hijack_weak  = scan_for_hijack_weak(path, parser, &hijackWeakDetails, verbose);
    bool isHijacked   = hijack_rpath || hijack_weak;

    // ─── 취약점 체크 ───
    char *vulnRpathDetails = NULL;
    char *vulnWeakDetails  = NULL;
    bool isVulnerableRpath = scan_for_vulnerable_rpath(path, parser, &vulnRpathDetails, verbose);
    BDBG("isVulnerableRpath=%d, details=%s", isVulnerableRpath, vulnRpathDetails ? vulnRpathDetails : "NULL");
    bool isVulnerableWeak  = scan_for_vulnerable_weak(path, parser, &vulnWeakDetails, verbose);
    bool isVulnerable      = isVulnerableRpath || isVulnerableWeak;
    BDBG("isVulnerable=%d (rpath=%d, weak=%d)", isVulnerable, isVulnerableRpath, isVulnerableWeak);

    // ─── 리포트 기록 (ScanReport API 사용) ───
    if (isHijacked || isVulnerable) {
        if (isHijacked) {
            if (hijack_rpath) {
                scan_report_add_issue(report, path,
                    ISSUE_HIJACKED, "RPATH 다중 후보 하이재킹",
                    hijackRpathDetails ? hijackRpathDetails : "rpath 하이재킹",
                    pid, processPath, processName);
            }
            if (hijack_weak) {
                scan_report_add_issue(report, path,
                    ISSUE_HIJACKED, "Weak Dylib 하이재킹",
                    hijackWeakDetails ? hijackWeakDetails : "weak dylib 하이재킹",
                    pid, processPath, processName);
            }
        }
        if (isVulnerable) {
            if (isVulnerableRpath) {
                BDBG("CALLING scan_report_add_issue for VULN_RPATH");
                scan_report_add_issue(report, path,
                    ISSUE_VULNERABLE, "RPATH 취약 경로",
                    vulnRpathDetails ? vulnRpathDetails : "rpath 취약점",
                    pid, processPath, processName);
            }
            if (isVulnerableWeak) {
                scan_report_add_issue(report, path,
                    ISSUE_VULNERABLE, "Weak Dylib 취약 경로",
                    vulnWeakDetails ? vulnWeakDetails : "weak dylib 취약점",
                    pid, processPath, processName);
            }
        }
    }

    // ─── 상세 문자열 해제 ───
    if (hijackRpathDetails) free(hijackRpathDetails);
    if (hijackWeakDetails)  free(hijackWeakDetails);
    if (vulnRpathDetails)   free(vulnRpathDetails);
    if (vulnWeakDetails)    free(vulnWeakDetails);

    free_macho_parser(parser);

    if (verbose) printf("[V] 스캔 완료: %s (hijacked=%d, vulnerable=%d)\n",
                        path, isHijacked, isVulnerable);
}