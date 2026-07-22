#include "path_utils.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>   // dirname(3) 사용 시 주의: 스레드 비안전/입력 수정 가능 → 자체 구현 유지

/* ──────────────────────────────────────────────
 * 외부 헬퍼: 경로 정규화(realpath) + 폴백
 * ────────────────────────────────────────────── */
char *normalize_path(const char *path) {
    if (!path) return NULL;
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) {
        return strdup(resolved);
    }
    // realpath 실패(권한 없음, 경로 일부 미존재 등) → 원본 복사 반환
    return strdup(path);
}

/* ──────────────────────────────────────────────
 * 파일 존재 여부 (정규화 후 확인)
 * ────────────────────────────────────────────── */
bool file_exists(const char *path) {
    if (!path) return false;
    char *norm = normalize_path(path);
    bool exists = (norm && access(norm, F_OK) == 0);
    free(norm);
    return exists;
}

/* ──────────────────────────────────────────────
 * 디렉터리 부분 추출 (루트 "/" 처리 포함)
 * ────────────────────────────────────────────── */
char *dirname_from_path(const char *path) {
    if (!path) return NULL;
    const char *lastSlash = strrchr(path, '/');
    if (!lastSlash) return strdup(".");
    if (lastSlash == path) return strdup("/");  // "/bin/ls" → "/"
    size_t dirLen = lastSlash - path;
    char *dir = malloc(dirLen + 1);
    if (!dir) return NULL;
    memcpy(dir, path, dirLen);
    dir[dirLen] = '\0';
    return dir;
}

/* ──────────────────────────────────────────────
 * @executable_path / @loader_path → 절대 경로 변환
 * ────────────────────────────────────────────── */
char *resolve_loader_executable_path(const char *binaryPath, const char *unresolvedPath) {
    if (!binaryPath || !unresolvedPath) return NULL;

    const char *execPrefix = "@executable_path";
    const char *loaderPrefix = "@loader_path";
    const char *prefix = NULL;

    if (strncmp(unresolvedPath, execPrefix, strlen(execPrefix)) == 0) {
        prefix = execPrefix;
    } else if (strncmp(unresolvedPath, loaderPrefix, strlen(loaderPrefix)) == 0) {
        prefix = loaderPrefix;
    } else {
        return normalize_path(unresolvedPath);  // 접두어 없으면 정규화만
    }

    char *binDir = dirname_from_path(binaryPath);
    if (!binDir) return NULL;

    const char *relativePart = unresolvedPath + strlen(prefix);

    // 안전하게 경로 합성 (snprintf)
    char combined[PATH_MAX];
    int n = snprintf(combined, sizeof(combined), "%s/%s", binDir,
                     (relativePart[0] == '/') ? relativePart + 1 : relativePart);
    free(binDir);
    if (n < 0 || n >= PATH_MAX) return NULL;

    return normalize_path(combined);
}

/* ──────────────────────────────────────────────
 * @rpath + LC_RPATH → 절대 경로 합성
 * ────────────────────────────────────────────── */
char *combine_rpath(const char *runPath, const char *dylibPath, const char *rpathPrefix) {
    if (!runPath || !dylibPath || !rpathPrefix) return NULL;

    size_t prefixLen = strlen(rpathPrefix);
    if (strncmp(dylibPath, rpathPrefix, prefixLen) != 0) return NULL;

    const char *relativePath = dylibPath + prefixLen;

    char combined[PATH_MAX];
    int n = snprintf(combined, sizeof(combined), "%s/%s", runPath,
                     (relativePath[0] == '/') ? relativePath + 1 : relativePath);
    if (n < 0 || n >= PATH_MAX) return NULL;

    return normalize_path(combined);
}