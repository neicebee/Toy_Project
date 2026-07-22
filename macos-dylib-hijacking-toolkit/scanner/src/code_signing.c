#include "code_signing.h"
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/proc_info.h>
#include <libproc.h>

/* ──────────────────────────────────────────────
 * 플래그 상수: SDK에 정의돼 있으면 사용, 없으면 폴백
 * ────────────────────────────────────────────── */
#ifndef kSecCodeInfoFlagHard
#define kSecCodeInfoFlagHard (1 << 15)
#endif

#ifndef kSecCodeInfoFlagLibraryValidated
#define kSecCodeInfoFlagLibraryValidated (1 << 17)
#endif

/* ──────────────────────────────────────────────
 * 핵심: 경로 기반 정적 서명 검증 및 정보 추출
 * ────────────────────────────────────────────── */
bool get_signing_info_for_path(const char *path, SigningInfo *info) {
    if (!path || !info) return false;

    CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(
        NULL, (const UInt8 *)path, strlen(path), false);
    if (!fileURL) return false;

    SecStaticCodeRef staticCode = NULL;
    OSStatus status = SecStaticCodeCreateWithPath(fileURL, kSecCSDefaultFlags, &staticCode);
    CFRelease(fileURL);
    if (status != errSecSuccess || !staticCode) return false;

    // ─── 서명 유효성 검사 ───
    // NOTE: kSecCSDoNotValidateResources로 리소스 디렉터리/Info.plist 검증은 생략 (속도 우선).
    // 완전 검증이 필요하면 별도 함수(예: get_signing_info_for_path_full)에서
    // kSecCSStrictValidate | kSecCSCheckNestedCode 등을 조합해 사용.
    status = SecStaticCodeCheckValidity(staticCode, kSecCSDoNotValidateResources, NULL);
    if (status != errSecSuccess) {
        CFRelease(staticCode);
        return false;
    }

    CFDictionaryRef signingDict = NULL;
    status = SecCodeCopySigningInformation(staticCode, kSecCSSigningInformation, &signingDict);
    if (status != errSecSuccess || !signingDict) {
        CFRelease(staticCode);
        return false;
    }

    // ─── 플래그 비트 추출 ───
    CFNumberRef flagsNumber = CFDictionaryGetValue(signingDict, kSecCodeInfoFlags);
    uint32_t flags = 0;
    if (flagsNumber) {
        CFNumberGetValue(flagsNumber, kCFNumberSInt32Type, &flags);
    }

    // ─── Apple 서명 여부 (anchor apple) ───
    SecRequirementRef appleReq = NULL;
    status = SecRequirementCreateWithString(CFSTR("anchor apple"), kSecCSDefaultFlags, &appleReq);
    bool appleSigned = false;
    if (status == errSecSuccess && appleReq) {
        status = SecStaticCodeCheckValidity(staticCode, kSecCSDefaultFlags, appleReq);
        appleSigned = (status == errSecSuccess);
        CFRelease(appleReq);
    }

    // ─── 엔타이틀먼트: disable-library-validation ───
    bool disableLibVal = false;
    CFDictionaryRef entitlementsDict = CFDictionaryGetValue(signingDict, kSecCodeInfoEntitlementsDict);
    if (entitlementsDict) {
        CFBooleanRef disableValRef = CFDictionaryGetValue(
            entitlementsDict, CFSTR("com.apple.security.cs.disable-library-validation"));
        if (disableValRef == kCFBooleanTrue) disableLibVal = true;
    }

    // ─── 결과 채우기 ───
    info->isApple               = appleSigned;
    info->hasHardenedRuntime    = ((flags & kSecCodeInfoFlagHard) != 0);
    info->hasLibraryValidation  = ((flags & kSecCodeInfoFlagLibraryValidated) != 0);
    info->disabledLibValidation = disableLibVal;

    CFRelease(signingDict);
    CFRelease(staticCode);
    return true;
}

/* ──────────────────────────────────────────────
 * PID 기반 래퍼 (기존 호환용)
 * ────────────────────────────────────────────── */
bool get_signing_info_for_pid(int pid, SigningInfo *info) {
    if (pid <= 0 || !info) return false;

    char path[PROC_PIDPATHINFO_MAXSIZE];
    int ret = proc_pidpath(pid, path, sizeof(path));
    if (ret <= 0) return false;

    return get_signing_info_for_path(path, info);
}

/* ──────────────────────────────────────────────
 * 기존 시그니처 호환 래퍼 (deprecated)
 * ────────────────────────────────────────────── */
bool get_signing_info(const char *path,
                      bool *isApple, bool *hardenedRuntime,
                      bool *libValidation, bool *disabledLibValidation) {
    SigningInfo info = {0};
    if (!get_signing_info_for_path(path, &info)) return false;

    if (isApple)             *isApple = info.isApple;
    if (hardenedRuntime)     *hardenedRuntime = info.hasHardenedRuntime;
    if (libValidation)       *libValidation = info.hasLibraryValidation;
    if (disabledLibValidation) *disabledLibValidation = info.disabledLibValidation;
    return true;
}