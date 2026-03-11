#include "vuln_scanner.h"
#include "path_utils.h"
#include "code_signing.h"
#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <mach-o/dyld.h>

// SIP 디렉터리 여부 확인
static bool is_SIP_directory(const char *path) {
    struct stat fileStat;
    if(stat(path, &fileStat)==0) { return (fileStat.st_flags&SF_RESTRICTED)!=0; }
    return false;   // stat 실패 시 SIP 보호 여부 판단 불가
}

// dyld 공유 캐시에 포함되어 있는지 확인
static bool is_incache(const char *path) {
    return _dyld_shared_cache_contains_path(path);
}

// [FILTER 2] Weak import 의심도 검증
bool is_weak_import_suspicious(const char *binaryPath, const char *weakDylibPath) {
    if (!binaryPath || !weakDylibPath) return false;
    
    // Apple 서명 dylib는 항상 신뢰 가능
    bool isApple = false;
    bool hardenedRuntime = false;
    bool libValidation = false;
    bool disabledLibValidation = false;
    
    if (get_signing_info(weakDylibPath, &isApple, &hardenedRuntime, &libValidation, &disabledLibValidation)) {
        if (isApple) {
            return false;  // Apple 서명이면 신뢰 가능
        }
    }
    
    // 부모 바이너리 서명 정보 확인
    bool parentIsApple = false;
    bool parentHardened = false;
    bool parentLibVal = false;
    bool parentDisabled = false;
    if (!get_signing_info(binaryPath, &parentIsApple, &parentHardened, &parentLibVal, &parentDisabled)) {
        return false;  // 부모 서명 정보 실패 시 의심하지 않음
    }
    
    // 부모가 서명되지 않음 → weak dylib 의심도 낮음
    if (parentDisabled) {
        return false;
    }
    
    // 부모가 서명됨 → weak dylib도 서명되어야 함
    bool weakIsApple = false;
    bool weakHardened = false;
    bool weakLibVal = false;
    bool weakDisabled = false;
    if (!get_signing_info(weakDylibPath, &weakIsApple, &weakHardened, &weakLibVal, &weakDisabled)) {
        return true;  // weak dylib 서명 정보 실패 → 의심!
    }
    
    // Weak dylib이 서명되지 않음 → 의심도 높음!
    if (weakDisabled) {
        return true;
    }
    
    return false;  // 둘 다 서명됨 → 의심도 낮음
}

// 취약점 후보인지 판단
static bool is_candidate(const char *path) {
    if(file_exists(path)) { return false; } // 파일 존재 시 후보 아님
    if(is_incache(path)) { return false; }  // dyld 공유 캐시 내 존재 시 후보 아님
    char *directory = strdup(path);
    if(!directory) { return false; }
    
    // 디렉터리 추출
    char *lastSlash = strrchr(directory, '/');
    if(lastSlash) { *lastSlash='\0'; } else { free(directory); return false; }
    
    // 상위 디렉터리 탐색
    while(!file_exists(directory)) {
        lastSlash = strrchr(directory, '/');
        if (!lastSlash || directory[0]=='\0' || strcmp(directory, "/")==0) { break; }
        *lastSlash = '\0';
    }
    
    // SIP 디렉터리 확인
    bool result = false;
    if(is_SIP_directory(directory)) { result = false; }
    else { result = true; }
    free(directory); return result;
}

bool scan_for_vulnerable_rpath(const char *path, MachOParser *parser, char **out_details) {
    if(out_details) *out_details = NULL;
    if(!parser || parser->lcRpathsCount==0 || parser->lcLoadDylibsCount==0) {
        return false;
    }
    const char *rpath_prefix = "@rpath";
    
    // collect candidate resolved paths
    char **foundPaths = NULL;
    size_t foundCount = 0;
    
    // @rpath 접두어를 가진 dylib 수집
    for (size_t i=0;i<parser->lcLoadDylibsCount;i++) {
        const char *dylibPath = parser->lcLoadDylibs[i];
        if (strncmp(dylibPath, rpath_prefix, strlen(rpath_prefix))!=0) { continue; }
        
        // rpath 순서 탐색(라이브러리 발견 위치 찾기)
        for (size_t j=0;j<parser->lcRpathsCount;j++) {
            const char *runPath = parser->lcRpaths[j];
            char *resolvedPath = combine_rpath(runPath, dylibPath, rpath_prefix);
            if (!resolvedPath) { continue; }
            if (g_verbose) {
                printf("[vuln] try resolve: %s + %s -> %s\n", runPath, dylibPath, resolvedPath);
            }
            if (strncmp(resolvedPath, "@executable_path", 16)==0 ||
                strncmp(resolvedPath, "@loader_path", 12)==0) {
                    char *absPath = resolve_loader_executable_path(path, resolvedPath);
                    free(resolvedPath);
                    resolvedPath = absPath;
                    if (g_verbose && resolvedPath) printf("[vuln] resolved to executable/loader path -> %s\n", resolvedPath);
                    if (!resolvedPath) { continue; }
            }
            
            // 최초 rpath라면 SIP 보호 여부 우선 확인
            if(j==0) {
                if(!is_SIP_directory(runPath)) {
                    // SIP 보호 받지 않는 최초 rpath 경로면 무조건 후보
                    if(g_verbose) {
                        printf("[vuln] first rpath not SIP protected, candidate path: %s\n", resolvedPath);
                    }
                    char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
                    if(tmp) {
                        foundPaths = tmp;
                        foundPaths[foundCount++] = resolvedPath;
                    } else {
                        free(resolvedPath);
                    }
                    break; // 이후 rpath 탐색 중단
                } else {    // SIP 보호 받는 최상위 rpath이면 기존 로직 진행
                    if(file_exists(resolvedPath)) {
                        free(resolvedPath); break; // 최초 rpath에서 dylib 발견 시 후보 등록 없이 탐색 종료
                    }
                    if(is_candidate(resolvedPath)) {
                        if(g_verbose) printf("[vuln] first rpath SIP protected candidate path: %s\n", resolvedPath);
                        char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
                        if (tmp) {
                            foundPaths = tmp;
                            foundPaths[foundCount++] = resolvedPath;
                        } else free(resolvedPath);
                        break;
                    } else {
                        free(resolvedPath); break;
                    }
                }
            } else {    // n번째 rpath 로직
                if(file_exists(resolvedPath)) {
                    if(g_verbose) printf("[info] dylib found at rpath #%zu: %s\n", j+1, resolvedPath);
                    if(is_candidate(resolvedPath)) {
                        if(g_verbose) printf("[vuln] candidate path: %s\n", resolvedPath);
                        char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
                        if(tmp) {
                            foundPaths = tmp;
                            foundPaths[foundCount++] = resolvedPath;
                        } else free(resolvedPath);
                    } else {
                        if(g_verbose) printf("[vuln] not candidate: %s\n", resolvedPath);
                        free(resolvedPath);
                    }
                    break;
                } else {
                    if(is_candidate(resolvedPath)) {
                        if(g_verbose) printf("[vuln] candidate path: %s\n", resolvedPath);
                        char **tmp = realloc(foundPaths, sizeof(char *) * (foundCount + 1));
                        if(tmp) {
                            foundPaths = tmp;
                            foundPaths[foundCount++] = resolvedPath;
                        } else free(resolvedPath);
                    } else {
                        if(g_verbose) printf("[vuln] not candidate: %s\n", resolvedPath);
                        free(resolvedPath); break;
                    }
                }
            }
        }
    }

    if (foundCount>0 && out_details) {
        size_t totalLen = 0;
        for (size_t k=0;k<foundCount;k++) totalLen += strlen(foundPaths[k]) + 3;
        char *details = malloc(totalLen + 1);
        if (details) {
            details[0] = '\0';
            for (size_t k=0;k<foundCount;k++) {
                strcat(details, foundPaths[k]);
                if (k+1<foundCount) strcat(details, "; ");
            }
            *out_details = details;
                if (g_verbose) printf("[vuln] report details: %s\n", details);
        }
    }

    for (size_t k=0;k<foundCount;k++) free(foundPaths[k]);
    free(foundPaths);

    return (foundCount>0);
}

bool scan_for_vulnerable_weak(const char *path, MachOParser *parser, char **out_details) {
    if (out_details) *out_details = NULL;
    if(!parser || parser->lcLoadWeakDylibsCount==0) {
        return false;
    }
    const char *rpath_prefix = "@rpath";

    char **foundPaths = NULL;
    size_t foundCount = 0;
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
                if(is_candidate(resolvedPath)) {
                    char **tmp = realloc(foundPaths, sizeof(char*) * (foundCount + 1));
                    if (tmp) {
                        foundPaths = tmp;
                        foundPaths[foundCount++] = resolvedPath;
                    } else { free(resolvedPath); }
                } else { free(resolvedPath); }
            }
        } else {
            char *resolvedPath = strdup(weakDylib);
            if(is_candidate(resolvedPath)) {
                char **tmp = realloc(foundPaths, sizeof(char*) * (foundCount + 1));
                if (tmp) {
                    foundPaths = tmp;
                    foundPaths[foundCount++] = resolvedPath;
                } else { free(resolvedPath); }
            } else { free(resolvedPath); }
        }
    }

    if (foundCount>0 && out_details) {
        size_t totalLen = 0;
        for (size_t k=0;k<foundCount;k++) totalLen += strlen(foundPaths[k]) + 3;
        char *details = malloc(totalLen + 1);
        if (details) {
            details[0] = '\0';
            for (size_t k=0;k<foundCount;k++) {
                strcat(details, foundPaths[k]);
                if (k+1<foundCount) strcat(details, "; ");
            }
            *out_details = details;
        }
    }

    for (size_t k=0;k<foundCount;k++) free(foundPaths[k]);
    free(foundPaths);

    return (foundCount>0);
}
