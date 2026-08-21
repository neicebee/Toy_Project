/**
 * framework_supply_chain.h
 *
 * Type B Framework 공격 모듈
 *
 * 기능:
 * - 앱의 Framework 분석
 * - Re-export dylib 자동 생성
 * - Framework 공격 배포 및 검증
 *
 * 사용 사례:
 * - IINA.app의 Sparkle.framework
 * - KakaoWork.app의 Sparkle.framework
 */

#ifndef FRAMEWORK_SUPPLY_CHAIN_H
#define FRAMEWORK_SUPPLY_CHAIN_H

#include <stdbool.h>
#include <stddef.h>

/* Supply Chain Framework 정보 */
typedef struct {
    char *path;              /* Framework 경로 */
    char *dylib_path;        /* 메인 dylib 경로 */
    char *name;              /* Framework 이름 */
    size_t shared_app_count; /* 이 Framework를 공유하는 앱 수 */
    char **shared_apps;      /* 공유 앱 목록 */
} SCFrameworkInfo;

/* Framework 공격 컨텍스트 */
typedef struct {
    char *target_app_path;           /* 대상 앱 경로 */
    char *target_framework_path;     /* 대상 Framework 경로 */
    SCFrameworkInfo *framework;      /* Framework 정보 */

    char *backup_dylib_path;         /* 백업 dylib 경로 */
    char *malicious_dylib_path;      /* 악성 dylib 경로 */
    char *attack_log_path;           /* 공격 로그 경로 */

    bool is_reexport_enabled;        /* Re-export 활성화 */
    bool is_deployed;                /* 배포 완료 */
} SupplyChainAttackContext;

/**
 * Supply Chain Framework 분석
 *
 * @param app_path 대상 앱 경로
 * @param framework_path Framework 경로
 * @return Framework 정보 또는 NULL
 */
SCFrameworkInfo* sc_analyze_framework(
    const char *app_path,
    const char *framework_path
);

/**
 * Re-export dylib 생성
 *
 * @param ctx Framework 공격 컨텍스트
 * @return 성공 시 true
 */
bool sc_generate_malicious_dylib_with_reexport(
    SupplyChainAttackContext *ctx
);

/**
 * Framework 공격 배포
 *
 * @param ctx Framework 공격 컨텍스트
 * @return 성공 시 true
 */
bool sc_deploy_supply_chain_attack(
    SupplyChainAttackContext *ctx
);

/**
 * 공격 검증
 *
 * @param ctx Framework 공격 컨텍스트
 * @param app_to_run 실행할 앱 경로
 * @param expected_keyword 예상 로그 키워드
 * @return 공격 성공 시 true
 */
bool sc_verify_supply_chain_attack(
    SupplyChainAttackContext *ctx,
    const char *app_to_run,
    const char *expected_keyword
);

/**
 * 공격 복구 (원본으로 복원)
 *
 * @param ctx Framework 공격 컨텍스트
 * @return 성공 시 true
 */
bool sc_restore_original_framework(
    SupplyChainAttackContext *ctx
);

/**
 * 컨텍스트 해제
 *
 * @param ctx Framework 공격 컨텍스트
 */
void sc_free_supply_chain_context(
    SupplyChainAttackContext *ctx
);

/**
 * Supply Chain Framework 정보 해제
 *
 * @param fw Supply Chain Framework 정보
 */
void sc_free_framework_info(
    SCFrameworkInfo *fw
);

#endif /* FRAMEWORK_SUPPLY_CHAIN_H */
