#ifndef HIJACK_DETECTOR_H
#define HIJACK_DETECTOR_H

#include "macho_parser.h"
#include <stdbool.h>

/**
 * 하이재킹 검사: @rpath 기반 LC_LOAD_DYLIB 경로들을 런타임 경로와 결합해 중복 존재 여부 확인
 * @param path 바이너리 파일 경로
 * @param parser Mach-O 파서
 * @return 하이재킹 탐지 여부
 */
bool scan_for_hijack_rpath(const char *path, MachOParser *parser, char **out_details);

/**
 * 하이재킹 검사: LC_LOAD_WEAK_DYLIB 스캔
 * @param path 바이너리 파일 경로
 * @param parser Mach-O 파서
 * @param out_details 출력할 상세 경로 문자열(동적할당, 호출자가 해제)
 * @return 하이재킹 탐지 여부
 */
bool scan_for_hijack_weak(const char *path, MachOParser *parser, char **out_details);

#endif // HIJACK_DETECTOR_H
