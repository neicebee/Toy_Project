#include "macho_parser.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach-o/loader.h>
#include <libkern/OSByteOrder.h>
#include <mach-o/fat.h>
#include <mach/machine.h>
#include <sys/sysctl.h>

/* ──────────────────────────────────────────────
 * 호스트 아키텍처 조회 헬퍼
 * ────────────────────────────────────────────── */
static cpu_type_t host_cpu_type(void) {
    cpu_type_t type;
    size_t len = sizeof(type);
    if (sysctlbyname("hw.cputype", &type, &len, NULL, 0) == 0) return type;
    return CPU_TYPE_ARM64;
}

static cpu_subtype_t host_cpu_subtype(void) {
    cpu_subtype_t subtype;
    size_t len = sizeof(subtype);
    if (sysctlbyname("hw.cpusubtype", &subtype, &len, NULL, 0) == 0) return subtype;
    return CPU_SUBTYPE_ARM64_ALL;
}

/* ──────────────────────────────────────────────
 * Fat/Universal Binary에서 호스트 아키텍처에 가장 적합한 슬라이스 선택
 * ────────────────────────────────────────────── */
static uint32_t select_best_fat_arch(const void *fatHeader, size_t fileSize) {
    const struct fat_header *fh = (const struct fat_header *)fatHeader;
    uint32_t nfat_arch = (fh->magic == FAT_CIGAM) ? OSSwapInt32(fh->nfat_arch) : fh->nfat_arch;
    if (nfat_arch == 0) return 0;

    const struct fat_arch *archs = (const struct fat_arch *)((const uint8_t *)fatHeader + sizeof(struct fat_header));
    cpu_type_t hostType = host_cpu_type();
    cpu_subtype_t hostSub = host_cpu_subtype();

    uint32_t bestOffset = 0;
    int bestScore = -1;

    for (uint32_t i = 0; i < nfat_arch; i++) {
        const struct fat_arch *a = &archs[i];
        cpu_type_t cputype = (fh->magic == FAT_CIGAM) ? OSSwapInt32(a->cputype) : a->cputype;
        cpu_subtype_t cpusubtype = (fh->magic == FAT_CIGAM) ? OSSwapInt32(a->cpusubtype) : a->cpusubtype;
        uint32_t offset = (fh->magic == FAT_CIGAM) ? OSSwapInt32(a->offset) : a->offset;
        uint32_t size = (fh->magic == FAT_CIGAM) ? OSSwapInt32(a->size) : a->size;

        if (offset + size > fileSize) continue;  // 경계 검사

        int score = 0;
        if (cputype == hostType) score += 10;
        if (cpusubtype == hostSub) score += 5;
        if (cputype == CPU_TYPE_ARM64 || cputype == CPU_TYPE_X86_64) score += 2;

        if (score > bestScore) {
            bestScore = score;
            bestOffset = offset;
        }
    }
    return bestOffset;
}

/* ──────────────────────────────────────────────
 * 공개 API: Mach-O 바이너리 파싱
 * ────────────────────────────────────────────── */
MachOParser* parse_binary(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }

    size_t fileSize = (size_t)st.st_size;
    void *fileMap = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (fileMap == MAP_FAILED) return NULL;

    uint8_t *buffer = (uint8_t *)fileMap;
    uint8_t *fileEnd = buffer + fileSize;  // ─── 경계 검사용 파일 끝 포인터

    // 파일 크기가 최소 4바이트(Magic)를 포함하는지 우선 확인
    if (buffer + sizeof(uint32_t) > fileEnd) {
        munmap(fileMap, fileSize);
        return NULL;
    }

    uint32_t magic = *(uint32_t *)buffer;
    uint8_t *machoStart = NULL;

    // Fat binary 검사
    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        uint32_t offset = select_best_fat_arch(buffer, fileSize);
        if (offset == 0 || offset >= fileSize) {
            munmap(fileMap, fileSize);
            return NULL;
        }
        machoStart = buffer + offset;
    } else {
        machoStart = buffer;
    }

    // Mach-O 헤더의 매직 넘버를 읽기 위한 4바이트 최소 공간 경계 검사 추가
    if (machoStart + sizeof(uint32_t) > fileEnd) {
        munmap(fileMap, fileSize);
        return NULL;
    }

    // Mach-O 헤더 검사
    magic = *(uint32_t *)machoStart;
    int is64bit = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);
    size_t headerSize = is64bit ? sizeof(struct mach_header_64) : sizeof(struct mach_header);
    
    // ─── [수정 반영] Mach-O 헤더 전체 크기에 대한 경계 검사 추가 ───
    if (machoStart + headerSize > fileEnd) {
        munmap(fileMap, fileSize);
        return NULL;
    }
    // ─────────────────────────────────────────────────────────────

    size_t ncmds = 0;

    if (magic == MH_MAGIC || magic == MH_MAGIC_64 || magic == MH_CIGAM || magic == MH_CIGAM_64) {
        if (is64bit) {
            struct mach_header_64 *mh = (struct mach_header_64 *)machoStart;
            ncmds = mh->ncmds;
        } else {
            struct mach_header *mh = (struct mach_header *)machoStart;
            ncmds = mh->ncmds;
        }
    } else {
        munmap(fileMap, fileSize);
        return NULL;
    }

    // 로드 커맨드 순회 포인터
    uint8_t *loadCmdPtr = machoStart + headerSize;

    MachOParser *parser = malloc(sizeof(MachOParser));
    if (!parser) {
        munmap(fileMap, fileSize);
        return NULL;
    }
    parser->path = strdup(path);
    parser->isParsed = 1;
    parser->lcLoadWeakDylibs = NULL;
    parser->lcLoadWeakDylibsCount = 0;
    parser->lcRpaths = NULL;
    parser->lcRpathsCount = 0;
    parser->lcLoadDylibs = NULL;
    parser->lcLoadDylibsCount = 0;

    size_t weakDylibCapacity = 8;
    size_t rpathCapacity = 8;
    size_t commonDylibCapacity = 8;

    parser->lcLoadWeakDylibs = malloc(sizeof(char*) * weakDylibCapacity);
    parser->lcRpaths = malloc(sizeof(char*) * rpathCapacity);
    parser->lcLoadDylibs = malloc(sizeof(char*) * commonDylibCapacity);

    for (size_t i = 0; i < ncmds; i++) {
        // ─── [3-1] 로드 커맨드 헤더가 파일 안에 있는지 확인
        if (loadCmdPtr + sizeof(struct load_command) > fileEnd) break;
        struct load_command *lc = (struct load_command *)loadCmdPtr;

        // ─── [3-2] cmdsize 유효성 검사
        if (lc->cmdsize < sizeof(struct load_command) ||
            loadCmdPtr + lc->cmdsize > fileEnd) break;

        if (lc->cmd == LC_LOAD_WEAK_DYLIB) {
            struct dylib_command *dylibCmd = (struct dylib_command *)lc;
            // ─── [3-3] 이름 오프셋 경계 검사
            uint32_t nameOffset = dylibCmd->dylib.name.offset;
            if (nameOffset >= lc->cmdsize ||
                (uint8_t *)dylibCmd + nameOffset >= fileEnd) {
                loadCmdPtr += lc->cmdsize;
                continue;
            }
            char *pathStr = (char *)dylibCmd + nameOffset;

            if (parser->lcLoadWeakDylibsCount == weakDylibCapacity) {
                weakDylibCapacity *= 2;
                parser->lcLoadWeakDylibs = realloc(parser->lcLoadWeakDylibs,
                                                   sizeof(char*) * weakDylibCapacity);
            }
            // ─── [4] strdup 실패 가드
            char *dup = strdup(pathStr);
            if (dup) {
                parser->lcLoadWeakDylibs[parser->lcLoadWeakDylibsCount++] = dup;
            }

        } else if (lc->cmd == LC_RPATH) {
            struct rpath_command *rpathCmd = (struct rpath_command *)lc;
            uint32_t pathOffset = rpathCmd->path.offset;
            if (pathOffset >= lc->cmdsize ||
                (uint8_t *)rpathCmd + pathOffset >= fileEnd) {
                loadCmdPtr += lc->cmdsize;
                continue;
            }
            char *pathStr = (char *)rpathCmd + pathOffset;

            if (parser->lcRpathsCount == rpathCapacity) {
                rpathCapacity *= 2;
                parser->lcRpaths = realloc(parser->lcRpaths, sizeof(char*) * rpathCapacity);
            }
            char *dup = strdup(pathStr);
            if (dup) {
                parser->lcRpaths[parser->lcRpathsCount++] = dup;
            }

        } else if (lc->cmd == LC_LOAD_DYLIB) {
            struct dylib_command *dylibCmd = (struct dylib_command *)lc;
            uint32_t nameOffset = dylibCmd->dylib.name.offset;
            if (nameOffset >= lc->cmdsize ||
                (uint8_t *)dylibCmd + nameOffset >= fileEnd) {
                loadCmdPtr += lc->cmdsize;
                continue;
            }
            char *pathStr = (char *)dylibCmd + nameOffset;

            if (parser->lcLoadDylibsCount == commonDylibCapacity) {
                commonDylibCapacity *= 2;
                parser->lcLoadDylibs = realloc(parser->lcLoadDylibs,
                                               sizeof(char*) * commonDylibCapacity);
            }
            char *dup = strdup(pathStr);
            if (dup) {
                parser->lcLoadDylibs[parser->lcLoadDylibsCount++] = dup;
            }
        }
        loadCmdPtr += lc->cmdsize;
    }

    munmap(fileMap, fileSize);
    return parser;
}

/* ──────────────────────────────────────────────
 * 파서 해제
 * ────────────────────────────────────────────── */
void free_macho_parser(MachOParser *parser) {
    if (!parser) return;

    free((void*)parser->path);

    for (size_t i = 0; i < parser->lcLoadWeakDylibsCount; i++) {
        free(parser->lcLoadWeakDylibs[i]);
    }
    free(parser->lcLoadWeakDylibs);

    for (size_t i = 0; i < parser->lcRpathsCount; i++) {
        free(parser->lcRpaths[i]);
    }
    free(parser->lcRpaths);

    for (size_t i = 0; i < parser->lcLoadDylibsCount; i++) {
        free(parser->lcLoadDylibs[i]);
    }
    free(parser->lcLoadDylibs);

    free(parser);
}

/* ──────────────────────────────────────────────
 * 빠른 Mach-O 판별 (magic bytes만 읽음)
 * ────────────────────────────────────────────── */
bool is_macho_executable(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    uint32_t magic;
    ssize_t r = read(fd, &magic, sizeof(magic));
    close(fd);
    if (r != sizeof(magic)) return false;

    if (magic == MH_MAGIC || magic == MH_CIGAM ||
        magic == MH_MAGIC_64 || magic == MH_CIGAM_64 ||
        magic == FAT_MAGIC || magic == FAT_CIGAM)
        return true;
    return false;
}