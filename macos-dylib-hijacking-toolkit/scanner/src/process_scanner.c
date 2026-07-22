#include "process_scanner.h"
#include "scan_report.h"
#include "binary_scanner.h"
#include <libproc.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define PROC_PATH_MAX  PROC_PIDPATHINFO_MAXSIZE

/* ──────────────────────────────────────────────
 * 단순 동적 배열로 중복 경로 관리 (연결 리스트 대체)
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
 * 공개 API
 * ────────────────────────────────────────────── */
ScanReport *scan_all_processes(bool verbose) {
    ScanReport *report = scan_report_new("processes", verbose);
    if (!report) return NULL;

    if (verbose) printf("[V] 프로세스 스캔 시작\n");

    // PID 목록 얻기
    int bufSize = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (bufSize <= 0) {
        if (verbose) fprintf(stderr, "[V] PID 버퍼 크기 획득 실패\n");
        return report;
    }

    int numPids = bufSize / sizeof(pid_t);
    pid_t *pids = calloc(numPids, sizeof(pid_t));
    if (!pids) {
        if (verbose) perror("[V] calloc");
        return report;
    }

    int ret = proc_listpids(PROC_ALL_PIDS, 0, pids, bufSize);
    if (ret <= 0) {
        if (verbose) fprintf(stderr, "[V] proc_listpids() 실패\n");
        free(pids);
        return report;
    }

    int procCount = ret / sizeof(pid_t);
    PathSet *scannedPaths = pathset_new();

    for (int i = 0; i < procCount; i++) {
        pid_t pid = pids[i];
        if (pid == 0) continue;

        char pathBuffer[PROC_PATH_MAX] = {0};
        int pathLen = proc_pidpath(pid, pathBuffer, sizeof(pathBuffer));
        if (pathLen <= 0 || pathBuffer[0] == '\0') continue;

        // 중복 경로 스킵
        if (pathset_contains(scannedPaths, pathBuffer)) continue;

        // 새 경로 등록
        pathset_add(scannedPaths, pathBuffer);

        if (verbose) printf("[V] 스캔 대상 프로세스: PID=%d, 경로=%s\n", pid, pathBuffer);

        // 프로세스 이름 추출
        const char *procName = strrchr(pathBuffer, '/');
        procName = procName ? procName + 1 : pathBuffer;

        // 단일 바이너리 스캔 (프로세스 정보 함께 전달)
        scan_binary(pathBuffer, report, verbose, pid, pathBuffer, procName);
    }

    free(pids);
    pathset_free(scannedPaths);

    if (verbose) printf("[V] 프로세스 스캔 완료 (총 %zu 바이너리)\n", report->count);
    return report;
}