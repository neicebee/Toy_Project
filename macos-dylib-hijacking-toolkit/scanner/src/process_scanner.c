#include "process_scanner.h"
#include "binary_scanner.h"
#include <libproc.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define PROC_PATH_MAX  PROC_PIDPATHINFO_MAXSIZE

// 단순 중복 검사용 노드 기반 연결 리스트
typedef struct PathNode {
    char *path;
    struct PathNode *next;
} PathNode;

// 연결 리스트 내 중복 검사 함수
static bool contains_path(PathNode *head, const char *path) {
    for (PathNode *cur=head; cur!=NULL; cur=cur->next) {
        if (strcmp(cur->path, path)==0) { return true; }
    }
    return false;
}

// 연결 리스트에 새 경로 저장
static bool add_path(PathNode **head, const char *path) {
    if (contains_path(*head, path)) { return false; } // 이미 존재
    PathNode *node = malloc(sizeof(PathNode));
    if (!node) return false;

    node->path = strdup(path);
    if (!node->path) { free(node); return false; }
    node->next = *head;
    *head = node;
    return true;
}

// 연결 리스트 메모리 해제
static void free_paths(PathNode *head) {
    while (head) {
        PathNode *tmp = head;
        head = head->next;
        free(tmp->path);
        free(tmp);
    }
}

ProcessReportArray* process_report_array_create(void) {
    ProcessReportArray *array = malloc(sizeof(ProcessReportArray));
    if (!array) return NULL;
    
    array->reports = malloc(sizeof(ProcessReport*) * 32);
    array->reportCount = 0;
    array->reportCapacity = 32;
    
    return array;
}

void process_report_array_add(ProcessReportArray *array, ProcessReport *report) {
    if (!array || !report) return;
    
    if (array->reportCount >= array->reportCapacity) {
        array->reportCapacity *= 2;
        array->reports = realloc(array->reports, sizeof(ProcessReport*) * array->reportCapacity);
    }
    
    array->reports[array->reportCount++] = report;
}

void process_report_array_free(ProcessReportArray *array) {
    if (!array) return;
    
    for (size_t i = 0; i < array->reportCount; i++) {
        process_report_free(array->reports[i]);
    }
    free(array->reports);
    free(array);
}

void process_report_array_print(ProcessReportArray *array, bool showOnlyIssues) {
    if (!array) return;
    
    printf("\n\n");
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                     스캔 결과 요약 리포트                            ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    
    size_t totalProcesses = array->reportCount;
    size_t issueProcesses = 0;
    size_t hijackingCount = 0;
    size_t vulnerabilityCount = 0;
    
    // 통계 계산
    for (size_t i = 0; i < array->reportCount; i++) {
        ProcessReport *report = array->reports[i];
        if (report->hasIssues) {
            issueProcesses++;
            for (size_t j = 0; j < report->issues.issueCount; j++) {
                BinaryIssue *issue = &report->issues.issues[j];
                if (issue->issueType == ISSUE_HIJACKED || issue->issueType == ISSUE_BOTH) {
                    hijackingCount++;
                }
                if (issue->issueType == ISSUE_VULNERABLE || issue->issueType == ISSUE_BOTH) {
                    vulnerabilityCount++;
                }
            }
        }
    }
    
    printf("\n[통계]\n");
    printf("  총 스캔 프로세스: %zu\n", totalProcesses);
    printf("  정상 프로세스: %zu\n", totalProcesses - issueProcesses);
    printf("  이상 감지 프로세스: %zu\n", issueProcesses);
    printf("    - 하이재킹: %zu\n", hijackingCount);
    printf("    - 취약점: %zu\n", vulnerabilityCount);
    
    printf("\n[상세 결과]\n");
    
    // 결과 출력
    for (size_t i = 0; i < array->reportCount; i++) {
        ProcessReport *report = array->reports[i];
        
        if (showOnlyIssues && !report->hasIssues) {
            continue;
        }
        
        process_report_print(report);
    }
    
    printf("\n");
}

ProcessReportArray* scan_all_processes(void) {
    ProcessReportArray *reportArray = process_report_array_create();
    if (!reportArray) return NULL;
    
    int bufSize = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (bufSize<=0) {
        fprintf(stderr, "Failed to get the size of PID buffer.\n");
        return reportArray;
    }

    int numPids = bufSize / sizeof(pid_t);
    pid_t *pids = calloc(numPids, sizeof(pid_t));
    if (!pids) {
        perror("calloc");
        return reportArray;
    }

    int ret = proc_listpids(PROC_ALL_PIDS, 0, pids, bufSize);
    if (ret<=0) {
        fprintf(stderr, "proc_listpids() failed.\n");
        free(pids);
        return reportArray;
    }

    // 실제 프로세스 수
    int procCount = ret/sizeof(pid_t);
    PathNode *scannedPaths = NULL;

    for (int i=0;i<procCount;i++) {
        if (pids[i]==0) continue;

        char pathBuffer[PROC_PATH_MAX] = {0};
        int pathLen = proc_pidpath(pids[i], pathBuffer, sizeof(pathBuffer));

        if (pathLen<=0 || strlen(pathBuffer)==0) { continue; }

        // 중복 확인 후 신규 경로면 스캔 처리
        if (!contains_path(scannedPaths, pathBuffer)) {
            if (add_path(&scannedPaths, pathBuffer)) {
                ProcessReport *report = process_report_create(pids[i], pathBuffer);
                if (report) {
                    scan_binary(pathBuffer, report);
                    process_report_array_add(reportArray, report);
                }
            }
        }
    }

    free(pids);
    free_paths(scannedPaths);
    
    return reportArray;
}
