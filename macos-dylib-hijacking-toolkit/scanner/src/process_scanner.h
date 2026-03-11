#ifndef PROCESS_SCANNER_H
#define PROCESS_SCANNER_H

#include "process_report.h"

/**
 * 프로세스 리포트 배열
 */
typedef struct {
    ProcessReport **reports;
    size_t reportCount;
    size_t reportCapacity;
} ProcessReportArray;

/**
 * ProcessReportArray 초기화
 * @return 초기화된 ProcessReportArray
 */
ProcessReportArray* process_report_array_create(void);

/**
 * ProcessReportArray에 리포트 추가
 * @param array ProcessReportArray
 * @param report 추가할 ProcessReport
 */
void process_report_array_add(ProcessReportArray *array, ProcessReport *report);

/**
 * ProcessReportArray 해제
 * @param array 해제할 ProcessReportArray
 */
void process_report_array_free(ProcessReportArray *array);

/**
 * 현재 시스템의 모든 실행 중인 프로세스를 열거하고 스캔
 * @return ProcessReportArray 형태의 스캔 결과
 */
ProcessReportArray* scan_all_processes(void);

/**
 * 모든 리포트 출력
 * @param array ProcessReportArray
 * @param showOnlyIssues true면 문제가 있는 프로세스만 출력
 */
void process_report_array_print(ProcessReportArray *array, bool showOnlyIssues);

#endif // PROCESS_SCANNER_H
