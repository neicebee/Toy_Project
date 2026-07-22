#ifndef BINARY_SCANNER_H
#define BINARY_SCANNER_H

#include "scan_report.h"
#include <stdbool.h>

/**
 * 단일 바이너리 스캔 (경로 기반)
 * @param path           바이너리 절대 경로 (realpath 결과 권장)
 * @param report         결과를 누적할 ScanReport
 * @param verbose        상세 로그 출력 여부
 * @param pid            프로세스 ID (번들 모드면 0)
 * @param processPath    프로세스 실행 파일 경로 (번들 모드면 NULL)
 * @param processName    프로세스 이름 (번들 모드면 NULL)
 */
void scan_binary(const char *path,
                 ScanReport *report,
                 bool verbose,
                 pid_t pid,
                 const char *processPath,
                 const char *processName);

/**
 * 보호된 디렉토리 여부 확인 (프라이버시 프롬프트 방지)
 * @param path 파일 경로
 * @return 보호된 디렉토리에 속하면 true
 */
bool is_protected_directory(const char *path);

#endif // BINARY_SCANNER_H