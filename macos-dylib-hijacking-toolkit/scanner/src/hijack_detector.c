#include "hijack_detector.h"
#include "code_signing.h"
#include "path_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────
 * 상수
 * ────────────────────────────────────────────── */
#define RPATH_PREFIX       "@rpath"
#define EXEC_PREFIX        "@executable_path"
#define LOADER_PREFIX      "@loader_path"
#define EXEC_PREFIX_LEN    (sizeof(EXEC_PREFIX) - 1)
#define LOADER_PREFIX_LEN  (sizeof(LOADER_PREFIX) - 1)

/* ──────────────────────────────────────────────
 * 내부 헬퍼: 단일 @rpath 의존성에 대한 실존 후보 경로 수집
 * ────────────────────────────────────────────── */
static size_t collect_existing_candidates(const char *binaryPath,
                                          const char *dylibPath,
                                          MachOParser *parser,
                                          char ***out_paths,
                                          bool verbose) {
    size_t foundCount = 0;
    char **foundPaths = NULL;

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
            printf("[hijack] try resolve: %s -> %s\n", dylibPath, resolvedPath);
        }

        if (file_exists(resolvedPath)) {
            if (verbose) printf("[hijack] found file: %s\n", resolvedPath);
            char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
            if (tmp) {
                foundPaths = tmp;
                foundPaths[foundCount++] = resolvedPath;  // ownership transfer
            } else {
                free(resolvedPath);
            }
        } else {
            free(resolvedPath);
        }
    }

    *out_paths = foundPaths;
    return foundCount;
}

/* ──────────────────────────────────────────────
 * 내부 헬퍼: 발견된 후보들로 상세 문자열 생성
 * ────────────────────────────────────────────── */
static char *build_details_string(char **paths, size_t count) {
    if (count == 0 || !paths) return NULL;
    size_t totalLen = 0;
    for (size_t i = 0; i < count; i++) totalLen += strlen(paths[i]) + 3;  // "; "
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
 * 공개 API: RPATH 기반 하이재킹 탐지
 * ────────────────────────────────────────────── */
bool scan_for_hijack_rpath(const char *path, MachOParser *parser,
                           char **out_details, bool verbose) {
    if (out_details) *out_details = NULL;
    if (!parser || parser->lcLoadDylibsCount == 0) return false;
    if (parser->lcRpathsCount == 0) return false;

    // 바이너리 서명 정보
    SigningInfo binInfo = {0};
    (void)get_signing_info_for_path(path, &binInfo);

    for (size_t i = 0; i < parser->lcLoadDylibsCount; i++) {
        const char *dylibPath = parser->lcLoadDylibs[i];
        if (strncmp(dylibPath, RPATH_PREFIX, strlen(RPATH_PREFIX)) != 0) continue;

        char **foundPaths = NULL;
        size_t foundCount = collect_existing_candidates(path, dylibPath, parser,
                                                        &foundPaths, verbose);

        // 복수 실존 후보 → 하이재킹 가능성
        if (foundCount >= 2) {
            // 첫 번째 후보의 서명 확인 (Apple 서명이면 무시)
            SigningInfo dylibInfo = {0};
            bool hasDylibInfo = false;
            if (foundCount > 0) {
                hasDylibInfo = get_signing_info_for_path(foundPaths[0], &dylibInfo);
            }
            if (hasDylibInfo && dylibInfo.isApple) {
                if (verbose) printf("[hijack] candidate dismissed: Apple-signed %s\n", foundPaths[0]);
                for (size_t k = 0; k < foundCount; k++) free(foundPaths[k]);
                free(foundPaths);
                continue;
            }
            // 바이너리와 dylib 서명 상태가 동일하면 오탐으로 간주
            if (hasDylibInfo &&
                binInfo.isApple == dylibInfo.isApple &&
                binInfo.disabledLibValidation == dylibInfo.disabledLibValidation) {
                if (verbose) printf("[hijack] candidate dismissed: signing matches for %s\n", foundPaths[0]);
                for (size_t k = 0; k < foundCount; k++) free(foundPaths[k]);
                free(foundPaths);
                continue;
            }

            // 상세 문자열 생성
            if (out_details) {
                *out_details = build_details_string(foundPaths, foundCount);
                if (verbose && *out_details) printf("[hijack] report details: %s\n", *out_details);
            }

            for (size_t k = 0; k < foundCount; k++) free(foundPaths[k]);
            free(foundPaths);
            return true;
        }

        // 후보 0~1개면 해제 후 계속
        for (size_t k = 0; k < foundCount; k++) free(foundPaths[k]);
        free(foundPaths);
    }
    return false;
}

/* ──────────────────────────────────────────────
 * 공개 API: Weak Dylib 하이재킹 탐지
 * ────────────────────────────────────────────── */
bool scan_for_hijack_weak(const char *path, MachOParser *parser,
                          char **out_details, bool verbose) {
    if (out_details) *out_details = NULL;
    if (!parser || parser->lcLoadWeakDylibsCount == 0) return false;

    char **foundPaths = NULL;
    size_t foundCount = 0;

    for (size_t i = 0; i < parser->lcLoadWeakDylibsCount; i++) {
        const char *weakDylib = parser->lcLoadWeakDylibs[i];

        if (strncmp(weakDylib, RPATH_PREFIX, strlen(RPATH_PREFIX)) == 0) {
            // @rpath 기반 weak dylib → RPATH 해석
            if (parser->lcRpathsCount > 0) {
                char **rpPaths = NULL;
                size_t rpCount = collect_existing_candidates(path, weakDylib, parser,
                                                             &rpPaths, verbose);
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
        } else {
            // 절대/상대 경로 weak dylib → 직접 존재 확인
            char *resolvedPath = strdup(weakDylib);
            if (file_exists(resolvedPath)) {
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

    // Apple 서명 dylib 필터링 (RPATH 기반 후보에 한해)
    // 간단히: 발견된 경로 중 Apple 서명이면 전체 후보에서 제외
    // (필요하면 후보별로 세분화 가능)
    bool hasAppleSigned = false;
    for (size_t k = 0; k < foundCount; k++) {
        SigningInfo info = {0};
        if (get_signing_info_for_path(foundPaths[k], &info) && info.isApple) {
            hasAppleSigned = true;
            if (verbose) printf("[hijack-weak] Apple-signed candidate dismissed: %s\n", foundPaths[k]);
            break;
        }
    }
    if (hasAppleSigned) {
        for (size_t k = 0; k < foundCount; k++) free(foundPaths[k]);
        free(foundPaths);
        if (out_details) *out_details = NULL;
        return false;
    }

    if (foundCount > 0 && out_details) {
        *out_details = build_details_string(foundPaths, foundCount);
    }

    for (size_t k = 0; k < foundCount; k++) free(foundPaths[k]);
    free(foundPaths);

    return (foundCount > 0);
}