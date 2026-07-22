#ifndef CODE_SIGNING_H
#define CODE_SIGNING_H

#include <stdbool.h>

typedef struct {
    bool isApple;                 // anchor apple 서명 여부
    bool hasHardenedRuntime;      // Hardened Runtime 플래그
    bool hasLibraryValidation;    // Library Validation 플래그
    bool disabledLibValidation;   // com.apple.security.cs.disable-library-validation 엔타이틀먼트
} SigningInfo;

/* 경로 기반: 정적 코드 서명 검증 + 서명 정보 추출
 * 반환: 성공 시 true, 실패 시 false (info는 미정의 상태) */
bool get_signing_info_for_path(const char *path, SigningInfo *info);

/* PID 기반: 기존 호환용 래퍼 (내부에서 경로 기반 함수 호출) */
bool get_signing_info_for_pid(int pid, SigningInfo *info);

/* 기존 시그니처 호환 래퍼 (deprecated) */
bool get_signing_info(const char *path,
                      bool *isApple, bool *hardenedRuntime,
                      bool *libValidation, bool *disabledLibValidation);

#endif // CODE_SIGNING_H