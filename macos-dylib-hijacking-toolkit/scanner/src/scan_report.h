#ifndef SCAN_REPORT_H
#define SCAN_REPORT_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

// ─── 단일 바이너리 이슈 ───
typedef enum {
    ISSUE_NONE = 0,
    ISSUE_HIJACKED = 1,
    ISSUE_VULNERABLE = 2,
    ISSUE_BOTH = 3
} BinaryIssueType;

typedef struct {
    char *binaryPath;         // 스캔된 바이너리 절대 경로 (realpath 결과)
    BinaryIssueType issueType;
    char *issueName;          // "RPATH 다중 후보 하이재킹" 등
    char *details;            // 상세 경로 문자열 등

    // ─── 프로세스 스캔 모드에서만 채워짐 (번들 모드면 NULL/0) ───
    pid_t pid;                // 0 = 미설정
    char *processPath;        // NULL = 미설정
    char *processName;        // NULL = 미설정
} BinaryReport;

// ─── 전체 스캔 결과 ───
typedef struct {
    BinaryReport *reports;    // 동적 배열
    size_t count;
    size_t capacity;

    // 메타데이터
    time_t scanTime;          // 스캔 시작 시각
    char *scanRoot;           // 번들 모드: 루트 디렉터리 / 프로세스 모드: "processes"
    bool verbose;             // 상세 로그 여부
} ScanReport;

/* ──────────────────────────────────────────────
 * 생성/해제
 * ────────────────────────────────────────────── */
ScanReport *scan_report_new(const char *scanRoot, bool verbose);
void scan_report_free(ScanReport *report);

/* ──────────────────────────────────────────────
 * 이슈 추가 (내부에서 BinaryReport 할당/확장)
 * ────────────────────────────────────────────── */
void scan_report_add_issue(ScanReport *report,
                           const char *binaryPath,
                           BinaryIssueType issueType,
                           const char *issueName,
                           const char *details,
                           pid_t pid,               // 0 이면 생략
                           const char *processPath, // NULL 이면 생략
                           const char *processName); // NULL 이면 생략

/* ──────────────────────────────────────────────
 * 출력
 * ────────────────────────────────────────────── */
void scan_report_print(const ScanReport *report, bool showProcessInfo);

/* ──────────────────────────────────────────────
 * 거짓 양성 필터 (기존 유지)
 * ────────────────────────────────────────────── */
bool is_false_positive(const char *dylib_path);

#endif // SCAN_REPORT_H