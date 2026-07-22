#ifndef VULN_SCANNER_H
#define VULN_SCANNER_H

#include "macho_parser.h"
#include <stdbool.h>

/**
 * 취약점 검사: @rpath 관련
 * @param path      바이너리 파일 경로
 * @param parser    Mach-O 파서
 * @param out_details 상세 경로 문자열(동적할당, 호출자 해제, "; " 구분)
 * @param verbose   상세 로그 출력 여부
 * @return 취약점 탐지 여부
 */
bool scan_for_vulnerable_rpath(const char *path, MachOParser *parser,
                               char **out_details, bool verbose);

/**
 * 취약점 검사: LC_LOAD_WEAK_DYLIB 관련
 * @param path      바이너리 파일 경로
 * @param parser    Mach-O 파서
 * @param out_details 상세 경로 문자열(동적할당, 호출자 해제, "; " 구분)
 * @param verbose   상세 로그 출력 여부
 * @return 취약점 탐지 여부
 */
bool scan_for_vulnerable_weak(const char *path, MachOParser *parser,
                              char **out_details, bool verbose);

/**
 * [FILTER 2] Weak import 의심도 검증
 * @param binaryPath 부모 바이너리 경로
 * @param weakDylibPath Weak dylib 경로
 * @return 의심도 높으면 true
 */
bool is_weak_import_suspicious(const char *binaryPath, const char *weakDylibPath);

#endif // VULN_SCANNER_H