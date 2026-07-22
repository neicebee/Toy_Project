#include "vuln_scanner.h"
#include "path_utils.h"
#include "code_signing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <mach-o/dyld.h>

/* ──────────────────────────────────────────────
 * 상수
 * ────────────────────────────────────────────── */
#define RPATH_PREFIX       "@rpath"
#define EXEC_PREFIX        "@executable_path"
#define LOADER_PREFIX      "@loader_path"
#define EXEC_PREFIX_LEN    (sizeof(EXEC_PREFIX) - 1)
#define LOADER_PREFIX_LEN  (sizeof(LOADER_PREFIX) - 1)

/* ──────────────────────────────────────────────
 * 내부 헬퍼: 경로의 상위 디렉터리 중 하나라도 SIP 보호면 true
 * ────────────────────────────────────────────── */
static bool is_path_sip_protected(const char *path) {
    if (!path) return false;
    char *norm = normalize_path(path);
    if (!norm) return false;

    char *dir = strdup(norm);
    free(norm);
    if (!dir) return false;

    char *lastSlash = strrchr(dir, '/');
    if (lastSlash) *lastSlash = '\0';
    else { free(dir); return false; }

    bool sip = false;
    while (dir[0] != '\0') {
        struct stat st;
        if (stat(dir, &st) == 0) {
            // 루트(/)는 SF_RESTRICTED가 걸려 있어도 SIP 보호 경로로 간주하지 않음
            if (strcmp(dir, "/") != 0 && (st.st_flags & SF_RESTRICTED)) {
                sip = true; break;
            }
        }
        if (strcmp(dir, "/") == 0) break;
        char *lastSlash = strrchr(dir, '/');
        if (lastSlash) *lastSlash = '\0';
        else break;
    }
    free(dir);
    return sip;
}

/* ──────────────────────────────────────────────
 * 내부 헬퍼: 취약 후보 경로 판별 (정규화 + 상위 쓰기 가능 + 비SIP)
 * ────────────────────────────────────────────── */
static bool is_candidate_path(const char *path) {
    if (!path) return false;
    if (file_exists(path)) return false;           // 이미 존재 → 후보 아님
    if (_dyld_shared_cache_contains_path(path)) return false;  // dyld 캐시 내 → 후보 아님

    char *norm = normalize_path(path);
    if (!norm) return false;

    char *dir = strdup(norm);
    free(norm);
    if (!dir) return false;

    // 디렉터리 부분만
    char *lastSlash = strrchr(dir, '/');
    if (lastSlash) *lastSlash = '\0';
    else { free(dir); return false; }

    // 상위 디렉터리 순회하며 쓰기 가능(존재) + 비SIP 찾기
    bool candidate = false;
    while (dir[0] != '\0') {
        if (file_exists(dir)) {           // 존재하는 디렉터리 발견
            if (!is_path_sip_protected(dir)) {
                candidate = true;         // 쓰기 가능 + 비SIP → 후보
            }
            break;                        // 더 이상 상위 안 봄
        }
        if (strcmp(dir, "/") == 0) break;
        lastSlash = strrchr(dir, '/');
        if (lastSlash) *lastSlash = '\0';
        else break;
    }
    free(dir);
    return candidate;
}

/* ──────────────────────────────────────────────
 * 내부 헬퍼: 상세 문자열 생성
 * ────────────────────────────────────────────── */
static char *build_details_string(char **paths, size_t count) {
    if (count == 0 || !paths) return NULL;
    size_t totalLen = 0;
    for (size_t i = 0; i < count; i++) totalLen += strlen(paths[i]) + 3;
    char *details = malloc(totalLen + 1);
    if (!details) return NULL;
    details[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        strcat(details, paths[i]);
        if (i + 1 < count) strcat(details, "; ");
    }
    return details;
}

/* ──────────────────────────────────────────────
 * 내부 헬퍼: 단일 @rpath 의존성에 대한 취약 후보 경로 수집
 * ────────────────────────────────────────────── */
#define VDBG(fmt, ...) if (verbose) printf("[DEBUG-VULN] " fmt "\n", ##__VA_ARGS__)
static size_t collect_vuln_candidates(const char *binaryPath,
                                      const char *dylibPath,
                                      MachOParser *parser,
                                      char ***out_paths,
                                      bool verbose) {
    size_t foundCount = 0;
    char **foundPaths = NULL;
    bool firstRpathMiss = false;   // 1번 RPATH에서 파일 미발견 여부

    for (size_t j = 0; j < parser->lcRpathsCount; j++) {
        const char *runPath = parser->lcRpaths[j];
        char *resolvedPath = combine_rpath(runPath, dylibPath, RPATH_PREFIX);
        if (!resolvedPath) continue;

        // @executable_path / @loader_path 추가 해석
        if (strncmp(resolvedPath, EXEC_PREFIX, EXEC_PREFIX_LEN) == 0 ||
            strncmp(resolvedPath, LOADER_PREFIX, LOADER_PREFIX_LEN) == 0) {
            char *absPath = resolve_loader_executable_path(binaryPath, resolvedPath);
            free(resolvedPath);
            resolvedPath = absPath;
            if (!resolvedPath) continue;
        }

        if (verbose) {
            printf("[vuln] try resolve: rpath[%zu]=%s -> %s\n", j, runPath, resolvedPath);
        }

        bool isCandidate = false;

        if (file_exists(resolvedPath)) {
            // ─── 실제 로드 위치 확정 ───
            if (verbose) printf("[vuln] dylib FOUND at rpath[%zu]: %s\n", j, resolvedPath);

            if (!is_path_sip_protected(resolvedPath)) {
                // 비 SIP에서 로드됨 → 취약!
                isCandidate = true;
                if (verbose) printf("[vuln] VULNERABLE: loaded from non-SIP path\n");
            } else {
                // SIP 보호 경로에서 로드됨 → 안전, 탐색 종료
                if (verbose) printf("[vuln] SAFE: loaded from SIP-protected path\n");
            }
            // 로드 위치 확정 → 후속 RPATH 의미 없음
            if (!isCandidate) { free(resolvedPath); break; }
        } else {
            // ─── 파일 없음(미스) ───
            if (j == 0) firstRpathMiss = true;

            if (is_candidate_path(resolvedPath)) {
                // 공격자가 심을 수 있는 쓰기 가능 비 SIP 경로
                isCandidate = true;
                if (verbose) printf("[vuln] CANDIDATE (missing but writable non-SIP): %s\n", resolvedPath);
            } else {
                if (verbose) printf("[vuln] not candidate: %s\n", resolvedPath);
            }
        }

        if (isCandidate) {
            VDBG("candidate ADD: %s (foundCount=%zu->%zu)", resolvedPath, foundCount, foundCount+1);
            char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
            if (tmp) {
                foundPaths = tmp;
                foundPaths[foundCount++] = resolvedPath;  // ownership transfer
            } else {
                VDBG("realloc FAILED for %s", resolvedPath);
                free(resolvedPath);
            }
        } else {
            VDBG("realloc FAILED for %s", resolvedPath);
            free(resolvedPath);
        }

        // 실제 로드 위치 확정 시(파일 존재) 루프 종료
        if (file_exists(resolvedPath)) {
            // 위 if 블록에서 이미 break 처리됨 (isCandidate 여부와 무관)
            // 여기 도달 안 함
        }
    }

    *out_paths = foundPaths;
    return foundCount;
}

/* ──────────────────────────────────────────────
 * 공개 API: RPATH 기반 취약점 탐지
 * ────────────────────────────────────────────── */
bool scan_for_vulnerable_rpath(const char *path, MachOParser *parser,
                               char **out_details, bool verbose) {
    if (out_details) *out_details = NULL;
    if (!parser || parser->lcRpathsCount == 0 || parser->lcLoadDylibsCount == 0) return false;

    char **foundPaths = NULL;
    size_t foundCount = 0;

    for (size_t i = 0; i < parser->lcLoadDylibsCount; i++) {
        const char *dylibPath = parser->lcLoadDylibs[i];
        if (strncmp(dylibPath, RPATH_PREFIX, strlen(RPATH_PREFIX)) != 0) continue;

        char **rpPaths = NULL;
        size_t rpCount = collect_vuln_candidates(path, dylibPath, parser, &rpPaths, verbose);

        // 병합
        for (size_t k = 0; k < rpCount; k++) {
            char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
            if (tmp) {
                foundPaths = tmp;
                foundPaths[foundCount++] = rpPaths[k];
            } else {
                free(rpPaths[k]);
            }
        }
        free(rpPaths);
    }

    if (foundCount > 0 && out_details) {
        *out_details = build_details_string(foundPaths, foundCount);
        if (verbose && *out_details) printf("[vuln] report details: %s\n", *out_details);
    }

    for (size_t k = 0; k < foundCount; k++) free(foundPaths[k]);
    free(foundPaths);

    return (foundCount > 0);
}

/* ──────────────────────────────────────────────
 * 공개 API: Weak Dylib 취약점 탐지
 * ────────────────────────────────────────────── */
bool scan_for_vulnerable_weak(const char *path, MachOParser *parser,
                              char **out_details, bool verbose) {
    if (out_details) *out_details = NULL;
    if (!parser || parser->lcLoadWeakDylibsCount == 0) return false;

    char **foundPaths = NULL;
    size_t foundCount = 0;

    for (size_t i = 0; i < parser->lcLoadWeakDylibsCount; i++) {
        const char *weakDylib = parser->lcLoadWeakDylibs[i];

        if (strncmp(weakDylib, RPATH_PREFIX, strlen(RPATH_PREFIX)) == 0) {
            if (parser->lcRpathsCount == 0) continue;

            char **rpPaths = NULL;
            size_t rpCount = collect_vuln_candidates(path, weakDylib, parser, &rpPaths, verbose);

            for (size_t k = 0; k < rpCount; k++) {
                char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
                if (tmp) {
                    foundPaths = tmp;
                    foundPaths[foundCount++] = rpPaths[k];
                } else {
                    free(rpPaths[k]);
                }
            }
            free(rpPaths);
        } else {
            // 절대/상대 경로 weak dylib → 직접 후보 판별
            char *resolvedPath = strdup(weakDylib);
            if (is_candidate_path(resolvedPath)) {
                char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
                if (tmp) {
                    foundPaths = tmp;
                    foundPaths[foundCount++] = resolvedPath;
                } else {
                    free(resolvedPath);
                }
            } else {
                free(resolvedPath);
            }
        }
    }

    // [FILTER 2] Weak import 의심도 검증 적용 (선택적)
    // 발견된 후보 중 is_weak_import_suspicious가 true인 것만 남기려면 여기서 필터링 가능

    if (foundCount > 0 && out_details) {
        *out_details = build_details_string(foundPaths, foundCount);
        if (verbose && *out_details) printf("[vuln-weak] report details: %s\n", *out_details);
    }

    for (size_t k = 0; k < foundCount; k++) free(foundPaths[k]);
    free(foundPaths);

    VDBG("RETURN: foundCount=%zu -> %s", foundCount, foundCount > 0 ? "true" : "false");
    return (foundCount > 0);
}

/* ──────────────────────────────────────────────
 * [FILTER 2] Weak import 의심도 검증 (SigningInfo 기반)
 * ────────────────────────────────────────────── */
bool is_weak_import_suspicious(const char *binaryPath, const char *weakDylibPath) {
    if (!binaryPath || !weakDylibPath) return false;

    SigningInfo binInfo = {0}, weakInfo = {0};
    bool hasBin = get_signing_info_for_path(binaryPath, &binInfo);
    bool hasWeak = get_signing_info_for_path(weakDylibPath, &weakInfo);

    // Apple 서명 weak dylib → 신뢰
    if (hasWeak && weakInfo.isApple) return false;

    // 부모 바이너리 서명 정보 없음 → 판단 불가
    if (!hasBin) return false;

    // 부모가 disable-library-validation → weak dylib 서명 강제 안 함 → 의심 낮음
    if (binInfo.disabledLibValidation) return false;

    // 부모가 서명됨(Apple 또는 유효한 개발자 ID)인데 weak dylib이 서명 안 됨 → 의심
    if ((binInfo.isApple || (hasBin && !binInfo.disabledLibValidation)) &&
        (!hasWeak || weakInfo.disabledLibValidation)) {
        return true;
    }

    return false;
}