#include "path_utils.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

bool file_exists(const char *path) {
    return access(path, F_OK)==0;
}

char *dirname_from_path(const char *path) {
    if (!path) return NULL;
    const char *lastSlash = strrchr(path, '/');
    if (!lastSlash) return strdup(".");
    size_t dirLen = lastSlash - path;
    char *dir = malloc(dirLen+1);
    if (!dir) return NULL;
    strncpy(dir, path, dirLen);
    dir[dirLen]='\0';
    return dir;
}

char *resolve_loader_executable_path(const char *binaryPath, const char *unresolvedPath) {
    const char *executablePrefix = "@executable_path";
    const char *loaderPrefix = "@loader_path";
    const char *prefix = NULL;

    if (strncmp(unresolvedPath, executablePrefix, strlen(executablePrefix)) == 0) {
        prefix = executablePrefix;
    } else if (strncmp(unresolvedPath, loaderPrefix, strlen(loaderPrefix)) == 0) {
        prefix = loaderPrefix;
    } else {
        // 접두어 없으면 원본 복사 반환
        return strdup(unresolvedPath);
    }

    char *binDir = dirname_from_path(binaryPath);
    if (!binDir) return NULL;

    const char *relativePart = unresolvedPath + strlen(prefix);

    // 경로 합성: 바이너리 경로 디렉터리 + '/' + 접두어 제거한 경로
    size_t len = strlen(binDir) + 1 + strlen(relativePart) + 1;
    char *resolved = malloc(len);
    if (!resolved) {
        free(binDir);
        return NULL;
    }

    strcpy(resolved, binDir);
    if (binDir[strlen(binDir) - 1] != '/') {
        strcat(resolved, "/");
    }
    if (relativePart[0] == '/') {
        strcat(resolved, relativePart + 1);
    } else {
        strcat(resolved, relativePart);
    }

    free(binDir);
    return resolved;
}

char *combine_rpath(const char *runPath, const char *dylibPath, const char *rpathPrefix) {
    size_t prefixLen = strlen(rpathPrefix);
    const char *relativePath = dylibPath + prefixLen;
    size_t len = strlen(runPath) + 1 + strlen(relativePath) + 1;
    char *resolvedPath = malloc(len);
    if (!resolvedPath) return NULL;

    strcpy(resolvedPath, runPath);
    if (runPath[strlen(runPath) - 1] != '/') {
        strcat(resolvedPath, "/");
    }
    if (relativePath[0] == '/') {
        strcat(resolvedPath, relativePath + 1);
    } else {
        strcat(resolvedPath, relativePath);
    }
    return resolvedPath;
}
