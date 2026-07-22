#ifndef PROCESS_SCANNER_H
#define PROCESS_SCANNER_H

#include "scan_report.h"   // ← ScanReport 정의 사용

/**
 * 현재 시스템의 모든 실행 중인 프로세스를 열거하고 스캔
 * @param verbose 상세 로그 출력 여부
 * @return ScanReport 형태의 스캔 결과 (호출자 해제 책임: scan_report_free)
 */
ScanReport *scan_all_processes(bool verbose);

#endif // PROCESS_SCANNER_H