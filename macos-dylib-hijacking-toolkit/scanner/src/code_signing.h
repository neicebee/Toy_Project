#ifndef CODE_SIGNING_H
#define CODE_SIGNING_H

#include <stdbool.h>

/**
 * 바이너리 코드 서명 정보 조회
 * @param path 바이너리 파일 경로
 * @param isApple Apple 서명 여부 (출력)
 * @param hardenedRuntime 강화된 런타임 사용 여부 (출력)
 * @param libValidation 라이브러리 검증 활성화 여부 (출력)
 * @param disabledLibValidation 라이브러리 검증 비활성화 여부 (출력)
 * @return 성공 여부
 */
bool get_signing_info(const char *path, bool *isApple, bool *hardenedRuntime,
                    bool *libValidation,
                    bool *disabledLibValidation);

#endif // CODE_SIGNING_H
