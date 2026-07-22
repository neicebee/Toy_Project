#include "scan_report.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// ─── 거짓 양성 목록 ───
static const char* KNOWN_FALSE_POSITIVES[] = {
    "/Applications/Microsoft Messenger.app/Contents/Frameworks/mbukernel.framework/Versions/14/mbukernel",
    "/Applications/Microsoft Office 2011/Office/mbuinstrument.framework/Versions/14/mbuinstrument",
    "/Applications/MATLAB_R2014b.app/cefclient/bin/maci64/cefclient.app/Contents/MacOS/libcef.dylib",
    "/Library/Frameworks/OSXFUSE.framework/Versions/A/OSXFUSE",
    "/Applications/SmartConverter.app/Contents/Frameworks/Sparkle.framework/Versions/A/Sparkle",
    NULL
};

bool is_false_positive(const char *dylib_path) {
    if (!dylib_path) return false;
    for (int i = 0; KNOWN_FALSE_POSITIVES[i]; i++) {
        if (strcmp(dylib_path, KNOWN_FALSE_POSITIVES[i]) == 0) return true;
    }
    return false;
}

/* ──────────────────────────────────────────────
 * ScanReport 생성/해제
 * ────────────────────────────────────────────── */
ScanReport *scan_report_new(const char *scanRoot, bool verbose) {
    ScanReport *r = calloc(1, sizeof(ScanReport));
    if (!r) return NULL;
    r->capacity = 32;
    r->reports = calloc(r->capacity, sizeof(BinaryReport));
    r->scanTime = time(NULL);
    r->scanRoot = scanRoot ? strdup(scanRoot) : NULL;
    r->verbose = verbose;
    return r;
}

void scan_report_free(ScanReport *r) {
    if (!r) return;
    for (size_t i = 0; i < r->count; i++) {
        BinaryReport *b = &r->reports[i];
        free(b->binaryPath);
        free(b->issueName);
        free(b->details);
        free(b->processPath);
        free(b->processName);
    }
    free(r->reports);
    free(r->scanRoot);
    free(r);
}

/* ──────────────────────────────────────────────
 * 이슈 추가: 동일 binaryPath가 이미 있으면 기존 BinaryReport에 이슈만 추가
 * ────────────────────────────────────────────── */
void scan_report_add_issue(ScanReport *r,
                           const char *binaryPath,
                           BinaryIssueType issueType,
                           const char *issueName,
                           const char *details,
                           pid_t pid,
                           const char *processPath,
                           const char *processName) {
    if (!r || !binaryPath) return;
    if (details && is_false_positive(details)) return;  // 거짓 양성 필터
                            
    // 기존 BinaryReport 찾기
    BinaryReport *target = NULL;
    for (size_t i = 0; i < r->count; i++) {
        if (strcmp(r->reports[i].binaryPath, binaryPath) == 0) {
            target = &r->reports[i];
            break;
        }
    }

    // 없으면 새로 만들기
    if (!target) {
        if (r->count == r->capacity) {
            r->capacity *= 2;
            r->reports = realloc(r->reports, r->capacity * sizeof(BinaryReport));
        }
        target = &r->reports[r->count++];
        memset(target, 0, sizeof(BinaryReport));
        target->binaryPath = strdup(binaryPath);
        target->pid = pid;
        target->processPath = processPath ? strdup(processPath) : NULL;
        target->processName = processName ? strdup(processName) : NULL;
    }

    // 이슈 타입 병합 (기존보다 심각한 쪽으로)
    if (issueType > target->issueType) target->issueType = issueType;

    // ─── [수정 반영] issueName 병합 ───
    if (issueName) {
        if (!target->issueName) {
            target->issueName = strdup(issueName);
        } else {
            // 중복된 이슈명이 아닐 경우에만 병합
            if (strstr(target->issueName, issueName) == NULL) {
                size_t newLen = strlen(target->issueName) + strlen(issueName) + 4; // " | " 포함
                char *newStr = malloc(newLen);
                if (newStr) {
                    snprintf(newStr, newLen, "%s | %s", target->issueName, issueName);
                    free(target->issueName);
                    target->issueName = newStr;
                }
            }
        }
    }

    // ─── [수정 반영] details 병합 ───
    if (details) {
        if (!target->details) {
            target->details = strdup(details);
        } else {
            // 중복된 상세 내용이 아닐 경우에만 병합 (가독성을 위해 줄바꿈 사용)
            if (strstr(target->details, details) == NULL) {
                size_t newLen = strlen(target->details) + strlen(details) + 2; // "\n" 포함
                char *newStr = malloc(newLen);
                if (newStr) {
                    snprintf(newStr, newLen, "%s\n%s", target->details, details);
                    free(target->details);
                    target->details = newStr;
                }
            }
        }
    }
}

/* ──────────────────────────────────────────────
 * 출력
 * ────────────────────────────────────────────── */
static const char *issue_type_str(BinaryIssueType t) {
    switch (t) {
        case ISSUE_HIJACKED:   return "하이재킹";
        case ISSUE_VULNERABLE: return "취약점";
        case ISSUE_BOTH:       return "하이재킹 + 취약점";
        default:               return "알 수 없음";
    }
}

void scan_report_print(const ScanReport *r, bool showProcessInfo) {
    if (!r) return;

    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("                    macOS Dylib Hijack Scan Report                    \n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("스캔 모드: %s\n", r->scanRoot ? r->scanRoot : "unknown");
    printf("스캔 시각: %s", ctime(&r->scanTime));
    printf("총 바이너리 수: %zu\n", r->count);
    printf("────────────────────────────────────────────────────────────────────\n");

    size_t totalIssues = 0;
    for (size_t i = 0; i < r->count; i++) {
        const BinaryReport *b = &r->reports[i];
        bool hasIssue = (b->issueType != ISSUE_NONE);
        if (hasIssue) totalIssues++;
    }
    printf("이슈 발견 바이너리: %zu\n", totalIssues);
    printf("════════════════════════════════════════════════════════════════════\n\n");

    for (size_t i = 0; i < r->count; i++) {
        const BinaryReport *b = &r->reports[i];
        if (b->issueType == ISSUE_NONE) continue;  // 정상 바이너리는 생략 (verbose면 출력)

        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("바이너리: %s\n", b->binaryPath);

        if (showProcessInfo && b->pid > 0) {
            printf("프로세스: %s (PID: %d)\n", b->processName ? b->processName : "?", b->pid);
            printf("프로세스 경로: %s\n", b->processPath ? b->processPath : "?");
        }

        printf("문제 타입: %s\n", issue_type_str(b->issueType));
        if (b->issueName) printf("문제명: %s\n", b->issueName);
        if (b->details)   printf("상세: %s\n", b->details); // 다중 출력을 위해 포맷 변경
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    }

    if (totalIssues == 0) {
        printf("✓ 모든 바이너리에서 이슈 없음\n\n");
    }
}