#ifndef PROCESS_REPORT_H
#define PROCESS_REPORT_H

#include <stdbool.h>
#include <sys/types.h>

// 바이너리 문제 타입
typedef enum {
    ISSUE_NONE = 0,
    ISSUE_HIJACKED = 1,      // 하이재킹 감지
    ISSUE_VULNERABLE = 2,    // 취약점 감지
    ISSUE_BOTH = 3           // 하이재킹과 취약점 모두
} BinaryIssueType;

// 단일 바이너리의 문제 정보
typedef struct {
    char *binaryPath;         // 바이너리 경로
    BinaryIssueType issueType; // 문제 타입
    char *issueName;           // 문제 이름/분류 (예: "RPATH 취약점")
    char *details;             // 상세 설명
} BinaryIssue;

// 프로세스의 문제 정보 배열
typedef struct {
    BinaryIssue *issues;      // 문제 정보 배열
    size_t issueCount;        // 문제 개수
    size_t issueCapacity;     // 배열 용량
} ProcessIssues;

// 프로세스 상태 구조체
typedef struct {
    pid_t pid;                 // 프로세스 ID
    char *processPath;         // 프로세스 경로
    char *processName;         // 프로세스 이름
    bool hasIssues;            // 문제 존재 여부
    ProcessIssues issues;      // 문제 정보들
} ProcessReport;

/**
 * ProcessReport 초기화
 * @param pid 프로세스 ID
 * @param processPath 프로세스 경로
 * @return 초기화된 ProcessReport
 */
ProcessReport* process_report_create(pid_t pid, const char *processPath);

/**
 * [FILTER 3] 거짓 양성 여부 확인
 * @param dylib_path Dylib 경로
 * @return 알려진 거짓 양성이면 true
 */
bool is_false_positive(const char *dylib_path);

/**
 * ProcessReport에 바이너리 문제 추가
 * @param report ProcessReport
 * @param binaryPath 바이너리 경로
 * @param issueType 문제 타입
 * @param details 상세 설명
 */
void process_report_add_issue(ProcessReport *report, const char *binaryPath, 
                             BinaryIssueType issueType, const char *issueName, const char *details);

/**
 * ProcessReport 해제
 * @param report 해제할 ProcessReport
 */
void process_report_free(ProcessReport *report);

/**
 * ProcessReport 출력 (콘솔)
 * @param report ProcessReport
 */
void process_report_print(ProcessReport *report);

#endif // PROCESS_REPORT_H
