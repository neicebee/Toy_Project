#ifndef DYLIB_MODIFIER_H
#define DYLIB_MODIFIER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * macOS dylib 의존성 수정 모듈
 * 
 * 사용 시나리오: 의존성 확인 후 결정하기
 * 
 * [시나리오 1] 우리 dylib이 자동으로 의존성을 처리하는 경우
 *   - copy_dylib()로 dylib 배치
 *   - change_install_name()으로 install_name 설정
 *   - check_dylib_dependencies()로 의존성 확인
 *   - 자동 처리 또는 dlopen 사용
 * 
 * [시나리오 2] 바이너리의 의존성을 명시적으로 변경
 *   patch_dependency(binary_path, old_lib, new_lib)
 * 
 * [시나리오 3] 의존성을 먼저 검색한 후 패치 (권장)
 *   find_dependency_by_name(dylib, "libname", &found)
 *   patch_dependency(dylib, found, new_path)
 * 
 * [시나리오 4] 자동 패치 (가장 간단함)
 *   auto_patch_dependency_if_found(dylib, "libname", new_path)
 */

/**
 * 동적 링크 라이브러리의 헤더 수정 관련 함수들
 */

/**
 * dylib 파일을 대상 경로로 복사합니다.
 * 
 * @param source_dylib 원본 dylib 경로
 * @param target_path 타겟 dylib 경로
 * @return 성공 시 true, 실패 시 false
 */
bool copy_dylib(const char *source_dylib, const char *target_path);

/**
 * dylib 또는 바이너리의 의존성을 변경합니다.
 * (install_name_tool -change 사용)
 * 
 * 예: install_name_tool -change /old/path/lib.dylib /new/path/lib.dylib target_binary
 * 
 * @param target_file 수정할 파일 (바이너리 또는 dylib)
 * @param old_dependency 찾을 원본 의존성 경로
 * @param new_dependency 대체할 새 의존성 경로
 * @return 성공 시 true
 */
bool patch_dependency(const char *target_file, 
                     const char *old_dependency, 
                     const char *new_dependency);

/**
 * dylib의 의존성을 확인합니다. (otool -L 사용)
 * 
 * @param dylib_path 확인할 dylib의 경로
 * @return 성공 시 true
 */
bool check_dylib_dependencies(const char *dylib_path);

/**
 * dylib에서 특정 라이브러리 이름을 가진 의존성을 검색합니다.
 * 
 * 예: "libswiftCore.dylib"를 찾으면 전체 경로 반환
 * @param dylib_path 검색할 dylib의 경로
 * @param lib_name 찾을 라이브러리 이름 (예: "libswiftCore.dylib")
 * @param out_found_path 찾은 전체 경로 (동적 할당, 호출자가 해제)
 * @return 찾으면 true, 못 찾으면 false
 */
bool find_dependency_by_name(const char *dylib_path, 
                            const char *lib_name, 
                            char **out_found_path);

/**
 * dylib에서 특정 의존성을 찾아 자동으로 패치합니다.
 * 의존성을 먼저 확인해서 존재하면 패치, 없으면 스킵합니다.
 * 
 * @param dylib_path 수정할 dylib 경로
 * @param old_lib_name 찾을 라이브러리 이름 (예: "libswiftCore.dylib")
 * @param new_dependency 새로운 의존성 경로
 * @return 패치됨(true), 의존성 없음(false), 또는 오류(-1)
 */
int auto_patch_dependency_if_found(const char *dylib_path, 
                                   const char *old_lib_name, 
                                   const char *new_dependency);

/**
 * dylib 파일의 install_name을 변경합니다.
 * install_name은 다른 바이너리가 이 dylib을 로드할 때 사용할 경로입니다.
 * 
 * @param dylib_path 수정할 dylib의 경로
 * @param new_install_name 새로운 install_name
 * @return 성공 시 true, 실패 시 false
 */
bool change_install_name(const char *dylib_path, const char *new_install_name);

/**
 * dylib의 현재 install_name을 읽습니다.
 * 반환된 문자열은 호출자가 해제해야 합니다.
 * 
 * @param dylib_path dylib의 경로
 * @return install_name (동적 할당됨, 호출자가 해제), 실패 시 NULL
 */
char* get_dylib_install_name(const char *dylib_path);

#endif // DYLIB_MODIFIER_H
