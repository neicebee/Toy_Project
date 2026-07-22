#ifndef MACHO_PARSER_H
#define MACHO_PARSER_H

#include <sys/types.h>
#include <mach/machine.h>
#include <stdbool.h>

// Mach-O 파서 인스턴스를 저장할 구조체
typedef struct {
    const char *path;
    bool isParsed;
    
    cpu_type_t cputype;       // 파싱된 슬라이스의 CPU 타입
    cpu_subtype_t cpusubtype; // CPU 서브타입
    
    // LC_LOAD_WEAK_DYLIBS 경로 배열 및 개수
    char **lcLoadWeakDylibs;
    size_t lcLoadWeakDylibsCount;

    // LC_RPATHS 경로 배열 및 개수
    char **lcRpaths;
    size_t lcRpathsCount;
    
    // LC_LOAD_DYLIBS 경로 배열 및 개수
    char **lcLoadDylibs;
    size_t lcLoadDylibsCount;
} MachOParser;

/**
 * Mach-O 바이너리 파싱
 * @param path 바이너리 파일 경로
 * @return MachOParser 인스턴스 (실패시 NULL)
 */
MachOParser* parse_binary(const char *path);

/**
 * MachOParser 인스턴스 해제
 * @param parser 해제할 파서
 */
void free_macho_parser(MachOParser *parser);

// 빠른 Mach-O 판별 (magic bytes만 읽음)
bool is_macho_executable(const char *path);

#endif // MACHO_PARSER_H
