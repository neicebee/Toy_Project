#ifndef HIJACK_DETECTOR_H
#define HIJACK_DETECTOR_H

#include "macho_parser.h"
#include <stdbool.h>

/**
 * 하이재킹 검사: @rpath 기반 LC_LOAD_DYLIB 다중 후보 탐지
 * @param path      바이너리 파일 경로
 * @param parser    Mach-O 파서
 * @param out_details 상세 경로 문자열(동적할당, 호출자 해제, "; " 구분)
 * @param verbose   상세 로그 출력 여부
 * @return 하이재킹 탐지 여부
 */
bool scan_for_hijack_rpath(const char *path, MachOParser *parser,
                           char **out_details, bool verbose);

/**
 * 하이재킹 검사: LC_LOAD_WEAK_DYLIB 실존 후보 탐지
 * @param path      바이너리 파일 경로
 * @param parser    Mach-O 파서
 * @param out_details 상세 경로 문자열(동적할당, 호출자 해제, "; " 구분)
 * @param verbose   상세 로그 출력 여부
 * @return 하이재킹 탐지 여부
 */
bool scan_for_hijack_weak(const char *path, MachOParser *parser,
                          char **out_details, bool verbose);

#endif // HIJACK_DETECTOR_H