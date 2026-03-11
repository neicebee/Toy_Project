#include "macho_parser.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

MachOParser* parse_binary(const char *path) {
    int fd = open(path, O_RDONLY);
    if(fd < 0) return NULL;

    struct stat st;
    if(fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }

    size_t fileSize = (size_t)st.st_size;
    void *fileMap = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if(fileMap == MAP_FAILED) return NULL;

    uint8_t *buffer = (uint8_t *)fileMap;

    uint32_t magic = *(uint32_t *)buffer;
    uint8_t *machoStart = NULL;

    // Fat binary 검사
    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        struct fat_header *fh = (struct fat_header *)buffer;
        uint32_t nfat_arch = ntohl(fh->nfat_arch);
        if (nfat_arch < 1) {
            munmap(fileMap, fileSize);
            return NULL;
        }
        struct fat_arch *archs = (struct fat_arch *)(buffer + sizeof(struct fat_header));
        uint32_t offset = ntohl(archs[0].offset);
        if (offset >= fileSize) {
            munmap(fileMap, fileSize);
            return NULL;
        }
        machoStart = buffer + offset;
    } else {
        machoStart = buffer;
    }

    // Mach-O 헤더 검사
    magic = *(uint32_t *)machoStart;
    int is64bit = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);
    size_t headerSize = is64bit ? sizeof(struct mach_header_64) : sizeof(struct mach_header);
    size_t ncmds = 0;
    if (magic == MH_MAGIC || magic == MH_MAGIC_64 || magic == MH_CIGAM || magic == MH_CIGAM_64) {
        if(is64bit) {
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

    uint8_t *loadCmdPtr = machoStart + headerSize;

    MachOParser *parser = malloc(sizeof(MachOParser));
    if(!parser) {
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

    // 임시 동적 배열 용량
    size_t weakDylibCapacity = 8;
    size_t rpathCapacity = 8;
    size_t commonDylibCapacity = 8;

    parser->lcLoadWeakDylibs = malloc(sizeof(char*) * weakDylibCapacity);
    parser->lcRpaths = malloc(sizeof(char*) * rpathCapacity);
    parser->lcLoadDylibs = malloc(sizeof(char*) * commonDylibCapacity);

    for(size_t i = 0; i < ncmds; i++) {
        struct load_command *lc = (struct load_command *)loadCmdPtr;

        if(lc->cmd == LC_LOAD_WEAK_DYLIB) {
            struct dylib_command *dylibCmd = (struct dylib_command *)lc;
            char *pathStr = (char *)dylibCmd + dylibCmd->dylib.name.offset;

            if(parser->lcLoadWeakDylibsCount == weakDylibCapacity) {
                weakDylibCapacity *= 2;
                parser->lcLoadWeakDylibs = realloc(parser->lcLoadWeakDylibs, sizeof(char*) * weakDylibCapacity);
            }
            parser->lcLoadWeakDylibs[parser->lcLoadWeakDylibsCount++] = strdup(pathStr);

        } else if(lc->cmd == LC_RPATH) {
            struct rpath_command *rpathCmd = (struct rpath_command *)lc;
            char *pathStr = (char *)rpathCmd + rpathCmd->path.offset;

            if(parser->lcRpathsCount == rpathCapacity) {
                rpathCapacity *= 2;
                parser->lcRpaths = realloc(parser->lcRpaths, sizeof(char*) * rpathCapacity);
            }
            parser->lcRpaths[parser->lcRpathsCount++] = strdup(pathStr);
        } else if(lc->cmd == LC_LOAD_DYLIB) {
            struct dylib_command *dylibCmd = (struct dylib_command *)lc;
            char *pathStr = (char *)dylibCmd + dylibCmd->dylib.name.offset;
            
            if(parser->lcLoadDylibsCount == commonDylibCapacity) {
                commonDylibCapacity*=2;
                parser->lcLoadDylibs = realloc(parser->lcLoadDylibs, sizeof(char*)*commonDylibCapacity);
            }
            parser->lcLoadDylibs[parser->lcLoadDylibsCount++] = strdup(pathStr);
        }
        loadCmdPtr += lc->cmdsize;
    }

    munmap(fileMap, fileSize);
    return parser;
}

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
