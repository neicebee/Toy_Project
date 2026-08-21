#ifndef FRAMEWORK_ANALYZER_H
#define FRAMEWORK_ANALYZER_H

#include <stdbool.h>
#include <stdlib.h>

/**
 * macOS Framework 번들 분석 모듈
 * 
 * Type B (서드파티 Framework)와 Type C (내부 모듈화 Framework) 분석용
 * 
 * 사용 예:
 * 1. 단일 Framework 분석
 *    FrameworkInfo *info = analyze_framework("/path/to/Sparkle.framework");
 *    print_framework_info(info);
 *    free_framework_info(info);
 * 
 * 2. 앱 번들 내 모든 Framework 분석
 *    size_t count = 0;
 *    FrameworkInfo **infos = analyze_frameworks_in_app("/Applications/MyApp.app", &count);
 *    for (size_t i = 0; i < count; i++) {
 *        print_framework_info(infos[i]);
 *    }
 *    free_all_framework_infos(infos, count);
 */

/**
 * Framework 정보 구조체
 * 
 * Framework의 구조, 의존성, RPATH 등을 포함하는 종합 정보
 */
typedef struct {
    char *path;                     // Framework 번들 전체 경로
    char *name;                     // Framework 이름 (예: "Sparkle")
    
    // Framework 내부의 dylib들
    char **dylibs;                  // dylib 경로 배열
    size_t dylib_count;
    
    // 외부 의존성 (다른 Framework, 시스템 라이브러리)
    char **dependencies;            // 의존성 경로 배열
    size_t dep_count;
    
    // Framework가 참조하는 다른 Framework들
    char **internal_frameworks;     // 내부 Framework 참조 배열
    size_t internal_fw_count;
    
    // RPATH 체인 정보 (다중 RPATH 환경 분석용)
    char **rpath_chain;            // RPATH 목록 (순서 유지)
    size_t rpath_count;
} FrameworkInfo;

/* ================================================================
 * 공개 API
 * ================================================================ */

/**
 * 단일 Framework 분석
 * 
 * @param framework_path .framework 번들 경로
 * @return FrameworkInfo* (실패 시 NULL)
 * 
 * 분석 항목:
 * - Framework 내부 dylib 열거
 * - dylib의 의존성 추출
 * - 내부 프레임워크 참조 찾기
 * - RPATH 체인 추출
 */
FrameworkInfo* analyze_framework(const char *framework_path);

/**
 * 앱 번들 내의 모든 Framework 분석
 * 
 * @param app_bundle_path /Applications/MyApp.app 경로
 * @param out_count 발견된 Framework 개수 (출력)
 * @return FrameworkInfo** 배열 (실패 시 NULL)
 * 
 * 동작:
 * - Contents/Frameworks 디렉토리 스캔
 * - 각 .framework 개별 분석
 * - 반환 배열은 호출자가 free_all_framework_infos로 해제
 */
FrameworkInfo** analyze_frameworks_in_app(
    const char *app_bundle_path,
    size_t *out_count
);

/**
 * Framework 내의 모든 dylib 열거
 * 
 * @param framework_path .framework 경로
 * @param out_count dylib 개수 (출력)
 * @return char** dylib 경로 배열
 * 
 * Framework 번들의 계층 구조 탐색
 * (Versions/A/FrameworkName 등의 경로 포함)
 */
char** list_dylibs_in_framework(const char *framework_path, size_t *out_count);

/**
 * Framework의 의존성 추출
 * 
 * @param framework_path .framework 경로
 * @param out_count 의존성 개수 (출력)
 * @return char** 의존성 경로 배열
 * 
 * otool -L을 사용하여 Framework의 주요 dylib의
 * 의존성 목록을 추출합니다.
 * 
 * 반환되는 의존성:
 * - 다른 Framework 경로
 * - 시스템 라이브러리 (@loader_path, @rpath 등)
 * - 절대 경로 의존성
 */
char** extract_framework_dependencies(const char *framework_path, size_t *out_count);

/**
 * Framework 정보 구조 해제
 */
void free_framework_info(FrameworkInfo *info);

/**
 * Framework 정보 배열 전체 해제
 */
void free_all_framework_infos(FrameworkInfo **infos, size_t count);

/**
 * Framework 정보 콘솔 출력 (디버깅용)
 */
void print_framework_info(FrameworkInfo *info);

#endif // FRAMEWORK_ANALYZER_H
