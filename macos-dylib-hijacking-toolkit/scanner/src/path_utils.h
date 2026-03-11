#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <stdbool.h>

/**
 * 파일 존재 여부 확인 함수
 * @param path 파일 경로
 * @return 존재 여부
 */
bool file_exists(const char *path);

/**
 * 바이너리 경로에서 디렉토리 부분만 추출
 * @param path 전체 경로
 * @return 디렉토리 부분 (동적 할당, 호출자가 해제해야 함)
 */
char *dirname_from_path(const char *path);

/**
 * '@executable_path' 또는 '@loader_path' 접두어를 절대 경로로 변환
 * @param binaryPath 실행 중인 바이너리 경로
 * @param unresolvedPath 변환할 경로 (접두어 포함)
 * @return 변환된 절대 경로 (동적 할당, 호출자가 해제해야 함)
 */
char *resolve_loader_executable_path(const char *binaryPath, const char *unresolvedPath);

/**
 * 런타임 경로(runPath)와 dylib 상대 경로를 합쳐 절대 경로 생성
 * @param runPath 런타임 경로
 * @param dylibPath dylib 상대 경로 (접두사 포함 가능)
 * @param rpathPrefix @rpath 같은 접두사
 * @return 합성된 경로 (동적 할당, 호출자가 해제해야 함)
 */
char *combine_rpath(const char *runPath, const char *dylibPath, const char *rpathPrefix);

#endif // PATH_UTILS_H
