#include "bundle_scanner.h"
#include "binary_scanner.h"
#include "scan_report.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>

/* ──────────────────────────────────────────────
 * 내부 유틸리티: 중복 경로 관리 (PathSet)
 * ────────────────────────────────────────────── */
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} PathSet;

static PathSet *pathset_new(void) {
    PathSet *s = calloc(1, sizeof(PathSet));
    s->capacity = 64;
    s->items = calloc(s->capacity, sizeof(char *));
    return s;
}

static void pathset_free(PathSet *s) {
    if (!s) return;
    for (size_t i = 0; i < s->count; i++) free(s->items[i]);
    free(s->items);
    free(s);
}

static bool pathset_contains(PathSet *s, const char *path) {
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->items[i], path) == 0) return true;
    }
    return false;
}

static void pathset_add(PathSet *s, const char *path) {
    if (s->count == s->capacity) {
        s->capacity *= 2;
        s->items = realloc(s->items, s->capacity * sizeof(char *));
    }
    s->items[s->count++] = strdup(path);
}

/* ──────────────────────────────────────────────
 * Mach-O 빠른 판별 (magic bytes만 읽음)
 * ────────────────────────────────────────────── */
static bool is_macho_executable(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    uint32_t magic;
    ssize_t r = read(fd, &magic, sizeof(magic));
    close(fd);
    if (r != sizeof(magic)) return false;

    // Mach-O (thin)
    if (magic == 0xFEEDFACE ||       // MH_MAGIC (32-bit)
        magic == 0xFEEDFACF ||       // MH_MAGIC_64 (64-bit)
        magic == 0xCEFAEDFE ||       // MH_CIGAM (32-bit swapped)
        magic == 0xCFFAEDFE)         // MH_CIGAM_64 (64-bit swapped)
        return true;

    // Universal / Fat binary
    if (magic == 0xCAFEBABE ||       // FAT_MAGIC
        magic == 0xBEBAFECA)         // FAT_CIGAM
        return true;

    return false;
}

/* ──────────────────────────────────────────────
 * 디렉터리 재귀 순회 + Mach-O면 scan_binary 호출
 * ────────────────────────────────────────────── */
static void scan_dir_recursive(const char *dirPath,
                               ScanReport *report,
                               PathSet *scannedSet,
                               bool verbose) {
    DIR *dir = opendir(dirPath);
    if (!dir) {
        if (verbose) fprintf(stderr, "[V] 디렉터리 열기 실패: %s\n", dirPath);
        return;
    }

    struct dirent *ent;
    char fullPath[PATH_MAX];

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        int len = snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, ent->d_name);
        if (len < 0 || (size_t)len >= sizeof(fullPath)) continue;

        struct stat st;
        if (lstat(fullPath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            // 심볼릭 링크 디렉터리는 따라가지 않음 (무한 루프 방지)
            if (!(st.st_mode & S_IFLNK)) {
                scan_dir_recursive(fullPath, report, scannedSet, verbose);
            }
        } else if (S_ISREG(st.st_mode)) {
            // 정규 파일만 검사
            char realPath[PATH_MAX];
            if (realpath(fullPath, realPath) == NULL) continue;

            if (pathset_contains(scannedSet, realPath)) continue;

            if (is_macho_executable(realPath)) {
                if (verbose) printf("[V] 스캔 대상 발견: %s\n", realPath);
                pathset_add(scannedSet, realPath);
                // 번들 모드: 프로세스 정보 없음 (0, NULL, NULL)
                scan_binary(realPath, report, verbose, 0, NULL, NULL);
            }
        }
        // 심볼릭 링크 파일은 realpath로 정규화돼서 타겟이 스캔됨
    }
    closedir(dir);
}

/* ──────────────────────────────────────────────
 * .app 번들 하나 스캔: 표준 서브디렉터리만 타깃
 * ────────────────────────────────────────────── */
static void scan_app_bundle(const char *appPath,
                            ScanReport *report,
                            PathSet *scannedSet,
                            bool verbose) {
    const char *subdirs[] = {
        "Contents/MacOS",
        "Contents/Frameworks",
        "Contents/XPCServices",
        "Contents/PlugIns",
        "Contents/Library/LoginItems",
        "Contents/Helpers",
        NULL
    };

    char subPath[PATH_MAX];
    for (int i = 0; subdirs[i]; i++) {
        snprintf(subPath, sizeof(subPath), "%s/%s", appPath, subdirs[i]);
        struct stat st;
        if (stat(subPath, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (verbose) printf("[V] 번들 서브디렉터리 스캔: %s\n", subPath);
            scan_dir_recursive(subPath, report, scannedSet, verbose);
        }
    }
}

/* ──────────────────────────────────────────────
 * 공개 API
 * ────────────────────────────────────────────── */
ScanReport *scan_bundle_directory(const char *rootDir, bool verbose) {
    if (!rootDir) return NULL;

    struct stat st;
    if (stat(rootDir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "[ERROR] 루트 디렉터리가 존재하지 않음: %s\n", rootDir);
        return NULL;
    }

    if (verbose) printf("[*] 번들 디렉터리 스캔 시작: %s\n", rootDir);

    ScanReport *report = scan_report_new(rootDir, verbose);
    if (!report) return NULL;

    PathSet *scannedSet = pathset_new();

    // rootDir 직하 .app 디렉터리만 진입
    DIR *dir = opendir(rootDir);
    if (!dir) {
        fprintf(stderr, "[ERROR] 디렉터리 열기 실패: %s\n", rootDir);
        pathset_free(scannedSet);
        scan_report_free(report);
        return NULL;
    }

    struct dirent *ent;
    char appPath[PATH_MAX];

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        // .app 확장자 디렉터리만 대상
        size_t len = strlen(ent->d_name);
        if (len < 5 || strcmp(ent->d_name + len - 4, ".app") != 0)
            continue;

        snprintf(appPath, sizeof(appPath), "%s/%s", rootDir, ent->d_name);

        struct stat appSt;
        if (lstat(appPath, &appSt) != 0 || !S_ISDIR(appSt.st_mode))
            continue;

        if (verbose) printf("[*] 앱 번들 발견: %s\n", appPath);
        scan_app_bundle(appPath, report, scannedSet, verbose);
    }
    closedir(dir);

    pathset_free(scannedSet);

    if (verbose) printf("[*] 번들 디렉터리 스캔 완료 (총 %zu 바이너리)\n", report->count);
    return report;
}