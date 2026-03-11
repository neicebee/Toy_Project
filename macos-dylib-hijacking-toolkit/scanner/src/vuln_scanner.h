#ifndef VULN_SCANNER_H
#define VULN_SCANNER_H

#include "macho_parser.h"
#include <stdbool.h>

/**
 * 취약점 검사: @rpath 관련
 * @param path 바이너리 파일 경로
 * @param parser Mach-O 파서
 * @return 취약점 탐지 여부
 */
bool scan_for_vulnerable_rpath(const char *path, MachOParser *parser, char **out_details);

/**
 * 취약점 검사: LC_LOAD_WEAK_DYLIB 관련
 * @param path 바이너리 파일 경로
 * @param parser Mach-O 파서
 * @param out_details 출력할 상세 경로 문자열(동적할당, 호출자가 해제)
 * @return 취약점 탐지 여부
 */
bool scan_for_vulnerable_weak(const char *path, MachOParser *parser, char **out_details);

/**
 * [FILTER 2] Weak import 의심도 검증
 * 부모 바이너리가 서명됨 → weak import도 서명되어야 의심 낮음
 * @param binaryPath 부모 바이너리 경로
 * @param weakDylibPath Weak dylib 경로
 * @return 의심도 높으면 true (거짓 양성 가능)
 */
bool is_weak_import_suspicious(const char *binaryPath, const char *weakDylibPath);

#endif // VULN_SCANNER_H
