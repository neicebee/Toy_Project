#include "code_signing.h"
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>

#ifndef kSecCodeInfoFlagHard
#define kSecCodeInfoFlagHard (1 << 15)
#endif

#ifndef kSecCodeInfoFlagLibraryValidated
#define kSecCodeInfoFlagLibraryValidated (1 << 17)
#endif

bool get_signing_info(const char *path, bool *isApple, bool *hardenedRuntime,
                    bool *libValidation,
                    bool *disabledLibValidation) {
    CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), false);
    if (!fileURL) return false;

    SecStaticCodeRef staticCode = NULL;
    OSStatus status = SecStaticCodeCreateWithPath(fileURL, kSecCSDefaultFlags, &staticCode);
    CFRelease(fileURL);
    if (status!=errSecSuccess || !staticCode) return false;

    // 서명 유효성 검사
    status = SecStaticCodeCheckValidity(staticCode, kSecCSDoNotValidateResources, NULL);
    if (status!=errSecSuccess) { CFRelease(staticCode); return false; }

    CFDictionaryRef signingDict = NULL;
    status = SecCodeCopySigningInformation(staticCode, kSecCSSigningInformation, &signingDict);
    if (status!=errSecSuccess || !signingDict) { CFRelease(staticCode); return false; }

    CFNumberRef flagsNumber = CFDictionaryGetValue(signingDict, kSecCodeInfoFlags);
    uint32_t flags = 0;
    if (flagsNumber) {
        CFNumberGetValue(flagsNumber, kCFNumberSInt32Type, &flags);
    }

    // Apple 바이너리 여부
    SecRequirementRef appleReq = NULL;
    status = SecRequirementCreateWithString(CFSTR("anchor apple"), kSecCSDefaultFlags, &appleReq);
    bool appleSigned = false;
    if (status==errSecSuccess && appleReq) {
        status = SecStaticCodeCheckValidity(staticCode, kSecCSDefaultFlags, appleReq);
        appleSigned = (status==errSecSuccess);
        CFRelease(appleReq);
    }

    // entitlements에서 disable library validation 확인
    bool disableLibVal = false;
    CFDictionaryRef entitlementsDict = CFDictionaryGetValue(signingDict, kSecCodeInfoEntitlementsDict);
    if (entitlementsDict) {
        CFBooleanRef disableValRef = CFDictionaryGetValue(entitlementsDict, CFSTR("com.apple.security.cs.disable-library-validation"));
        if (disableValRef==kCFBooleanTrue) { disableLibVal = true; }
    }

    *isApple = appleSigned;
    *hardenedRuntime = (flags & kSecCodeSignatureAdhoc)==0 && ((flags & kSecCodeInfoFlagHard)!=0);
    *libValidation = (flags & kSecCodeInfoFlagLibraryValidated)!=0;
    *disabledLibValidation = disableLibVal;

    CFRelease(signingDict);
    CFRelease(staticCode);

    return true;
}
