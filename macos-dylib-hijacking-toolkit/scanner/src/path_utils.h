#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <stdbool.h>

// 경로 정규화(realpath) + 폴백 (vuln_scanner 등에서 사용)
char *normalize_path(const char *path);

/**
 * 파일 존재 여부 확인
 * @param path 파일 경로
 * @return 존재하면 true
 */
bool file_exists(const char *path);

/**
 * 경로에서 디렉터리 부분 추출
 * @param path 전체 경로
 * @return 디렉터리 경로 (동적 할당, 호출자 해제)
 *         - "/bin/ls" → "/"
 *         - "relative/path" → "."
 */
char *dirname_from_path(const char *path);

/**
 * @executable_path / @loader_path 접두어를 절대 경로로 변환
 * @param binaryPath 기준이 되는 바이너리 경로
 * @param unresolvedPath 변환할 경로 (접두어 포함 가능)
 * @return 절대 경로 (동적 할당, 호출자 해제)
 *         - 내부에서 realpath로 정규화/심볼릭링크 해결 수행
 *         - 단일 바이너리 스캔 컨텍스트에서 @loader_path는 @executable_path와 동일하게 처리
 */
char *resolve_loader_executable_path(const char *binaryPath, const char *unresolvedPath);

/**
 * 런타임 경로(runPath)와 @rpath 상대 경로 합성
 * @param runPath LC_RPATH 값 (예: @executable_path/../Frameworks)
 * @param dylibPath LC_LOAD_DYLIB 값 (예: @rpath/libfoo.dylib)
 * @param rpathPrefix "@rpath" 등 접두사
 * @return 합성된 절대 경로 (동적 할당, 호출자 해제)
 *         - 내부에서 realpath로 정규화/심볼릭링크 해결 수행
 */
char *combine_rpath(const char *runPath, const char *dylibPath, const char *rpathPrefix);

#endif // PATH_UTILS_H