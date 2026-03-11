#include "hijack_detector.h"
#include "code_signing.h"
#include "path_utils.h"
#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool scan_for_hijack_rpath(const char *path, MachOParser *parser, char **out_details) {
    if (out_details) *out_details = NULL;
    if(!parser || parser->lcLoadDylibsCount==0) {
        return false;
    }
    if(!parser->lcRpathsCount) {
        return false;
    }
    const char *rpath_prefix = "@rpath";

    // 바이너리 서명 정보 미리 조회
    bool bIsApple = false, bHardenedRuntime = false, bLibValidation = false, bDisabledLibValidation = false;
    (void)get_signing_info(path, &bIsApple, &bHardenedRuntime, &bLibValidation, &bDisabledLibValidation);

    for (size_t i=0;i<parser->lcLoadDylibsCount;i++) {
        const char *dylibPath = parser->lcLoadDylibs[i];
        if (strncmp(dylibPath, rpath_prefix, strlen(rpath_prefix))!=0) { continue; }

        size_t foundCount = 0;
        // collect found paths
        char **foundPaths = NULL;
        size_t foundPathsCount = 0;
        for (size_t j=0;j<parser->lcRpathsCount;j++) {
            const char *runPath = parser->lcRpaths[j];
            char *resolvedPath = combine_rpath(runPath, dylibPath, rpath_prefix);
            if (!resolvedPath) { continue; }
            if (g_verbose) {
                printf("[hijack] try resolve: %s + %s -> %s\n", runPath, dylibPath, resolvedPath);
            }
            if (strncmp(resolvedPath, "@executable_path", 16)==0 ||
                strncmp(resolvedPath, "@loader_path", 12)==0) {
                    char *absPath = resolve_loader_executable_path(path, resolvedPath);
                    free(resolvedPath);
                    resolvedPath = absPath;
                    if (g_verbose && resolvedPath) {
                        printf("[hijack] resolved to executable/loader path -> %s\n", resolvedPath);
                    }
                    if (!resolvedPath) { continue; }
                }
            // 파일 존재 여부 체크
            if (file_exists(resolvedPath)) {
                if (g_verbose) printf("[hijack] found file: %s\n", resolvedPath);
                foundCount++;
                // append to foundPaths
                char **tmp = realloc(foundPaths, sizeof(char*) * (foundPathsCount + 1));
                if (tmp) {
                    foundPaths = tmp;
                    foundPaths[foundPathsCount++] = resolvedPath; // take ownership
                } else {
                    free(resolvedPath);
                }
            } else {
                free(resolvedPath);
            }
        }

        // 복수 발견 시 하이재킹 가능성 판단
        if (foundCount >= 2) {
            // dylib 서명 정보 확인
            bool dIsApple = false, dHardenedRuntime = false, dLibValidation = false, dDisabledLibValidation = false;
            if (foundPathsCount>0) {
                (void)get_signing_info(foundPaths[0], &dIsApple, &dHardenedRuntime, &dLibValidation, &dDisabledLibValidation);
            }
            // Apple 공식 서명된 dylib이면 건너뜀
            if (dIsApple) {
                if (g_verbose) printf("[hijack] candidate dismissed: Apple-signed %s\n", foundPaths[0]);
                for (size_t k=0;k<foundPathsCount;k++) free(foundPaths[k]);
                free(foundPaths);
                continue;
            }
            // 바이너리와 dylib 서명 상태 정상이면 오탐으로 판단
            if (bIsApple == dIsApple && bDisabledLibValidation == dDisabledLibValidation) {
                if (g_verbose) printf("[hijack] candidate dismissed: signing matches for %s\n", foundPaths[0]);
                for (size_t k=0;k<foundPathsCount;k++) free(foundPaths[k]);
                free(foundPaths);
                continue;
            }
            // prepare details string listing foundPaths
            if (out_details && foundPathsCount>0) {
                size_t totalLen = 0;
                for (size_t k=0;k<foundPathsCount;k++) totalLen += strlen(foundPaths[k]) + 3;
                char *details = malloc(totalLen + 1);
                if (details) {
                    details[0] = '\0';
                    for (size_t k=0;k<foundPathsCount;k++) {
                        strcat(details, foundPaths[k]);
                        if (k+1<foundPathsCount) strcat(details, "; ");
                    }
                    *out_details = details;
                    if (g_verbose) printf("[hijack] report details: %s\n", details);
                }
            }
            for (size_t k=0;k<foundPathsCount;k++) free(foundPaths[k]);
            free(foundPaths);
            return true;
        }
        for (size_t k=0;k<foundPathsCount;k++) free(foundPaths[k]);
        free(foundPaths);
    }
    return false;
}

bool scan_for_hijack_weak(const char *path, MachOParser *parser, char **out_details) {
    if (out_details) *out_details = NULL;
    if(!parser || parser->lcLoadWeakDylibsCount==0) {
        return false;
    }
    const char *rpath_prefix = "@rpath";

    (void)get_signing_info(path, &(bool){0}, &(bool){0}, &(bool){0}, &(bool){0});

    // collect candidate found paths for weak dylibs
    char **foundPaths = NULL;
    size_t foundPathsCount = 0;

    for (size_t i = 0; i < parser->lcLoadWeakDylibsCount; i++) {
        const char *weakDylib = parser->lcLoadWeakDylibs[i];
        if (strncmp(weakDylib, rpath_prefix, strlen(rpath_prefix))==0) {
            if (parser->lcRpathsCount == 0) { continue; }
            for (size_t j=0;j<parser->lcRpathsCount;j++) {
                const char *runPath = parser->lcRpaths[j];
                char *resolvedPath = combine_rpath(runPath, weakDylib, rpath_prefix);
                if (!resolvedPath) { continue; }
                if (strncmp(resolvedPath, "@executable_path", 16)==0 ||
                    strncmp(resolvedPath, "@loader_path", 12)==0) {
                        char *absPath = resolve_loader_executable_path(path, resolvedPath);
                        free(resolvedPath);
                        resolvedPath = absPath;
                        if (!resolvedPath) { continue; }
                    }
                if (file_exists(resolvedPath)) {
                    char **tmp = realloc(foundPaths, sizeof(char*) * (foundPathsCount + 1));
                    if (tmp) {
                        foundPaths = tmp;
                        foundPaths[foundPathsCount++] = resolvedPath;
                    } else {
                        free(resolvedPath);
                    }
                } else {
                    free(resolvedPath);
                }
            }
        } else {
            char *resolvedPath = strdup(weakDylib);
            if (file_exists(resolvedPath)) {
                char **tmp = realloc(foundPaths, sizeof(char*) * (foundPathsCount + 1));
                if (tmp) {
                    foundPaths = tmp;
                    foundPaths[foundPathsCount++] = resolvedPath;
                } else {
                    free(resolvedPath);
                }
            } else {
                free(resolvedPath);
            }
        }
    }

    if (foundPathsCount>0 && out_details) {
        size_t totalLen = 0;
        for (size_t k=0;k<foundPathsCount;k++) totalLen += strlen(foundPaths[k]) + 3;
        char *details = malloc(totalLen + 1);
        if (details) {
            details[0] = '\0';
            for (size_t k=0;k<foundPathsCount;k++) {
                strcat(details, foundPaths[k]);
                if (k+1<foundPathsCount) strcat(details, "; ");
            }
            *out_details = details;
        }
    }

    for (size_t k=0;k<foundPathsCount;k++) free(foundPaths[k]);
    free(foundPaths);

    return (foundPathsCount>0);
}
