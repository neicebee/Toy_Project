#include "process_report.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// [FILTER 3] 알려진 거짓 양성 목록
static const char* KNOWN_FALSE_POSITIVES[] = {
    "/Applications/Microsoft Messenger.app/Contents/Frameworks/mbukernel.framework/Versions/14/mbukernel",
    "/Applications/Microsoft Office 2011/Office/mbuinstrument.framework/Versions/14/mbuinstrument",
    "/Applications/MATLAB_R2014b.app/cefclient/bin/maci64/cefclient.app/Contents/MacOS/libcef.dylib",
    "/Library/Frameworks/OSXFUSE.framework/Versions/A/OSXFUSE",
    "/Applications/SmartConverter.app/Contents/Frameworks/Sparkle.framework/Versions/A/Sparkle",
    NULL  // 종료 마커
};

// [FILTER 3] 거짓 양성 여부 확인
bool is_false_positive(const char *dylib_path) {
    if (!dylib_path) return false;
    
    for (int i = 0; KNOWN_FALSE_POSITIVES[i] != NULL; i++) {
        if (strcmp(dylib_path, KNOWN_FALSE_POSITIVES[i]) == 0) {
            return true;  // 알려진 거짓 양성
        }
    }
    
    return false;  // 거짓 양성 아님
}

// 프로세스 이름만 추출 (경로에서)
static char* extract_process_name(const char *path) {
    if (!path) return strdup("Unknown");
    const char *lastSlash = strrchr(path, '/');
    if (lastSlash) {
        return strdup(lastSlash + 1);
    }
    return strdup(path);
}

ProcessReport* process_report_create(pid_t pid, const char *processPath) {
    ProcessReport *report = malloc(sizeof(ProcessReport));
    if (!report) return NULL;

    report->pid = pid;
    report->processPath = strdup(processPath);
    report->processName = extract_process_name(processPath);
    report->hasIssues = false;
    
    report->issues.issues = malloc(sizeof(BinaryIssue) * 8);
    report->issues.issueCount = 0;
    report->issues.issueCapacity = 8;

    return report;
}

void process_report_add_issue(ProcessReport *report, const char *binaryPath,
                             BinaryIssueType issueType, const char *issueName, const char *details) {
    if (!report) return;

    // [FILTER 3] 거짓 양성 필터링
    if (details && is_false_positive(details)) {
        return;  // 거짓 양성이면 보고 제외
    }

    // 용량 확장
    if (report->issues.issueCount >= report->issues.issueCapacity) {
        report->issues.issueCapacity *= 2;
        report->issues.issues = realloc(report->issues.issues,
                                       sizeof(BinaryIssue) * report->issues.issueCapacity);
    }

    BinaryIssue *issue = &report->issues.issues[report->issues.issueCount++];
    issue->binaryPath = strdup(binaryPath);
    issue->issueType = issueType;
    issue->issueName = strdup(issueName ? issueName : "");
    issue->details = strdup(details ? details : "");

    report->hasIssues = true;
}

void process_report_free(ProcessReport *report) {
    if (!report) return;

    free(report->processPath);
    free(report->processName);

    for (size_t i = 0; i < report->issues.issueCount; i++) {
        free(report->issues.issues[i].binaryPath);
        free(report->issues.issues[i].issueName);
        free(report->issues.issues[i].details);
    }
    free(report->issues.issues);

    free(report);
}

static const char* issue_type_to_string(BinaryIssueType type) {
    switch (type) {
        case ISSUE_HIJACKED:
            return "하이재킹";
        case ISSUE_VULNERABLE:
            return "취약점";
        case ISSUE_BOTH:
            return "하이재킹 + 취약점";
        default:
            return "알 수 없음";
    }
}

void process_report_print(ProcessReport *report) {
    if (!report) return;

    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("프로세스: %s (PID: %d)\n", report->processName, report->pid);
    printf("경로: %s\n", report->processPath);

    if (!report->hasIssues) {
        printf("상태: ✓ 정상\n");
    } else {
        printf("상태: ⚠ 이상 감지\n");
        printf("────────────────────────────────────────────────────────────────\n");
        printf("문제 수: %zu\n", report->issues.issueCount);
        printf("문제 목록:\n");
        for (size_t i = 0; i < report->issues.issueCount; i++) {
            BinaryIssue *issue = &report->issues.issues[i];
            printf("  [%zu] 바이너리: %s\n", i + 1, issue->binaryPath);
            printf("       - 문제 타입: %s\n", issue_type_to_string(issue->issueType));
            if (issue->issueName && strlen(issue->issueName)>0) {
                printf("       - 문제명: %s\n", issue->issueName);
            }
            if (issue->details && strlen(issue->details) > 0) {
                printf("       - 상세: %s\n", issue->details);
            }
        }
        printf("────────────────────────────────────────────────────────────────\n");
    }
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}
