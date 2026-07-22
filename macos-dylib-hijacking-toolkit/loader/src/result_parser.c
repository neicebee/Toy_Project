#include "result_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>

#define MAX_LINE_LENGTH 8192
#define INITIAL_CAPACITY 16

/* ------------------------------------------------------------
 * 내부 유틸
 * ------------------------------------------------------------ */
static inline char *trim(char *s) {
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e >= s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

static char *normalize_path(const char *path) {
    if (!path) return NULL;
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) return strdup(resolved);
    return strdup(path);
}

/* ------------------------------------------------------------
 * VulnerableTarget 생성/해제
 * ------------------------------------------------------------ */
static VulnerableTarget *target_new(const char *binary_path) {
    VulnerableTarget *t = calloc(1, sizeof(VulnerableTarget));
    t->binary_path = normalize_path(binary_path);
    t->vuln_type = VULN_NONE;
    return t;
}

static void target_free(VulnerableTarget *t) {
    if (!t) return;
    free(t->binary_path);
    for (size_t i = 0; i < t->rpath_count; i++) free(t->rpath_vulns[i]);
    free(t->rpath_vulns);
    for (size_t i = 0; i < t->weak_dylib_count; i++) free(t->weak_dylib_vulns[i]);
    free(t->weak_dylib_vulns);
    free(t);
}

static void results_ensure_cap(ParsedResults *res) {
    if (res->target_count >= res->capacity) {
        res->capacity = (res->capacity == 0) ? 8 : res->capacity * 2;
        res->targets = realloc(res->targets, res->capacity * sizeof(VulnerableTarget *));
    }
}

/* ------------------------------------------------------------
 * 세미콜론(;) 분리 → 정규화 → pathvec 푸시
 * ------------------------------------------------------------ */
static void parse_and_push_paths(const char *detail_str,
                                 char ***vec, size_t *cnt, size_t *cap) {
    if (!detail_str || !*detail_str) return;
    char *copy = strdup(detail_str);
    char *save = NULL;
    char *tok = strtok_r(copy, ";", &save);
    while (tok) {
        char *p = trim(tok);
        if (*p) {
            char *norm = normalize_path(p);
            if (norm) {
                if (*cnt >= *cap) {
                    *cap = (*cap == 0) ? 8 : *cap * 2;
                    *vec = realloc(*vec, *cap * sizeof(char *));
                }
                (*vec)[(*cnt)++] = norm;
            }
        }
        tok = strtok_r(NULL, ";", &save);
    }
    free(copy);
}

/* ------------------------------------------------------------
 * 타겟 최종 확정 처리
 * ------------------------------------------------------------ */
static void finalize_target(VulnerableTarget *cur, const char *rpath_buf, const char *weak_buf, ParsedResults *res) {
    if (!cur) return;

    if (cur->vuln_type != VULN_NONE) {
        if (rpath_buf && rpath_buf[0]) {
            parse_and_push_paths(rpath_buf, &cur->rpath_vulns, &cur->rpath_count, &cur->rpath_cap);
        }
        if (weak_buf && weak_buf[0]) {
            parse_and_push_paths(weak_buf, &cur->weak_dylib_vulns, &cur->weak_dylib_count, &cur->weak_dylib_cap);
        }
        results_ensure_cap(res);
        res->targets[res->target_count++] = cur;
    } else {
        target_free(cur);
    }
}

/* ------------------------------------------------------------
 * 메인 파서
 * ------------------------------------------------------------ */
ParsedResults *parse_scanner_output(const char *output_file) {
    FILE *fp = fopen(output_file, "r");
    if (!fp) {
        fprintf(stderr, "[ERROR] 파일을 열 수 없음: %s\n", output_file);
        return NULL;
    }

    ParsedResults *res = calloc(1, sizeof(ParsedResults));
    res->capacity = INITIAL_CAPACITY;
    res->targets = calloc(res->capacity, sizeof(VulnerableTarget *));

    char line[MAX_LINE_LENGTH];
    VulnerableTarget *cur = NULL;
    
    char rpath_buf[8192] = "";
    char weak_buf[8192] = "";
    int detail_line_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') line[len - 1] = '\0';

        /* 0) 구분선(━, ═), 문제 타입 및 완료 푸터 무시 */
        if (strstr(line, "━") || strstr(line, "═") || strstr(line, "문제 타입:") || strstr(line, "스캔 완료")) {
            detail_line_count = 0; // 구분선이나 헤더를 만나면 상세 경로 수집 중단
            continue;
        }

        /* 1) 새 바이너리 시작 지점 */
        if (strstr(line, "바이너리:") == line) {
            if (cur) {
                finalize_target(cur, rpath_buf, weak_buf, res);
            }
            
            char *p = strchr(line, ':');
            if (p) {
                p++; while (isspace((unsigned char)*p)) p++;
                cur = target_new(p);
            }
            rpath_buf[0] = '\0';
            weak_buf[0] = '\0';
            detail_line_count = 0;
            continue;
        }

        if (!cur) continue;

        /* 2) 문제명 감지 */
        if (strstr(line, "문제명:")) {
            if (strstr(line, "RPATH")) cur->vuln_type |= VULN_RPATH;
            if (strstr(line, "Weak Dylib")) cur->vuln_type |= VULN_WEAK_DYLIB;
            continue;
        }

        /* 3) 상세 경로 첫번째 라인 (상세:) */
        if (strstr(line, "상세:")) {
            detail_line_count = 1;
            char *p = strchr(line, ':');
            if (p) { 
                p++; while (isspace((unsigned char)*p)) p++; 
                if (*p) {
                    if (cur->vuln_type == VULN_BOTH || (cur->vuln_type & VULN_RPATH)) {
                        if (rpath_buf[0]) strncat(rpath_buf, ";", sizeof(rpath_buf) - strlen(rpath_buf) - 1);
                        strncat(rpath_buf, p, sizeof(rpath_buf) - strlen(rpath_buf) - 1);
                    } else if (cur->vuln_type & VULN_WEAK_DYLIB) {
                        if (weak_buf[0]) strncat(weak_buf, ";", sizeof(weak_buf) - strlen(weak_buf) - 1);
                        strncat(weak_buf, p, sizeof(weak_buf) - strlen(weak_buf) - 1);
                    }
                }
            }
            continue;
        }

        /* 4) 상세 경로 두번째 이상 라인 (\n 구분) */
        if (detail_line_count > 0) {
            char *trimmed = trim(line);
            if (trimmed[0] != '\0') {
                detail_line_count++;
                if (cur->vuln_type == VULN_BOTH) {
                    if (weak_buf[0]) strncat(weak_buf, ";", sizeof(weak_buf) - strlen(weak_buf) - 1);
                    strncat(weak_buf, trimmed, sizeof(weak_buf) - strlen(weak_buf) - 1);
                } else if (cur->vuln_type & VULN_RPATH) {
                    if (rpath_buf[0]) strncat(rpath_buf, ";", sizeof(rpath_buf) - strlen(rpath_buf) - 1);
                    strncat(rpath_buf, trimmed, sizeof(rpath_buf) - strlen(rpath_buf) - 1);
                } else if (cur->vuln_type & VULN_WEAK_DYLIB) {
                    if (weak_buf[0]) strncat(weak_buf, ";", sizeof(weak_buf) - strlen(weak_buf) - 1);
                    strncat(weak_buf, trimmed, sizeof(weak_buf) - strlen(weak_buf) - 1);
                }
            }
        }
    }

    /* 5) EOF 도달 시 마지막 타겟 확정 */
    if (cur) {
        finalize_target(cur, rpath_buf, weak_buf, res);
    }

    fclose(fp);
    return res;
}

/* ------------------------------------------------------------
 * 결과 해제 / 출력
 * ------------------------------------------------------------ */
void free_parsed_results(ParsedResults *res) {
    if (!res) return;
    for (size_t i = 0; i < res->target_count; i++) target_free(res->targets[i]);
    free(res->targets);
    free(res);
}

void print_parsed_results(ParsedResults *res) {
    if (!res) return;
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("               파싱된 취약한 바이너리 정보 (로더용)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    printf("발견된 취약한 바이너리: %zu개\n\n", res->target_count);
    for (size_t i = 0; i < res->target_count; i++) {
        VulnerableTarget *t = res->targets[i];
        printf("[%zu] %s\n", i + 1, t->binary_path);
        printf("    취약점 타입: ");
        if (t->vuln_type == VULN_RPATH) printf("RPATH만\n");
        else if (t->vuln_type == VULN_WEAK_DYLIB) printf("Weak Dylib만\n");
        else if (t->vuln_type == VULN_BOTH) printf("RPATH + Weak Dylib\n");
        else printf("알 수 없음\n");
        if (t->rpath_count) {
            printf("    [RPATH 취약 경로 (%zu개)]\n", t->rpath_count);
            for (size_t j = 0; j < t->rpath_count; j++) printf("      - %s\n", t->rpath_vulns[j]);
        }
        if (t->weak_dylib_count) {
            printf("    [Weak Dylib 취약 경로 (%zu개)]\n", t->weak_dylib_count);
            for (size_t j = 0; j < t->weak_dylib_count; j++) printf("      - %s\n", t->weak_dylib_vulns[j]);
        }
        printf("\n");
    }
}