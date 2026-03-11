#ifndef BINARY_SCANNER_H
#define BINARY_SCANNER_H

#include "process_report.h"

/**
 * 단일 바이너리 스캔
 * 바이너리의 Mach-O 구조를 파싱하고 하이재킹 및 취약점 검사
 * @param path 바이너리 파일 경로
 * @param report ProcessReport에 결과 저장 (선택사항, NULL 가능)
 * @return 문제 감지 여부
 */
bool scan_binary(const char *path, ProcessReport *report);

/**
 * 보호된 디렉토리 여부 확인 (프라이버시 프롬프트 방지)
 * @param path 파일 경로
 * @return 보호된 디렉토리에 속하면 true
 */
bool is_protected_directory(const char *path);

#endif // BINARY_SCANNER_H
