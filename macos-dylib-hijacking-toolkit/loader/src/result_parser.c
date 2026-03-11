#include "result_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 4096
#define INITIAL_CAPACITY 10

/**
 * 취약한 경로 배열에 경로 추가
 */
static void add_vulnerability_path(char ***paths, size_t *count, size_t *capacity, const char *path) {
    if (*count >= *capacity) {
        *capacity = (*capacity + 1) * 2;
        *paths = realloc(*paths, *capacity * sizeof(char *));
    }
    
    // 경로의 공백 제거 (trim)
    const char *start = path;
    while (isspace(*start)) start++;
    
    const char *end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    
    size_t len = end - start + 1;
    (*paths)[*count] = malloc(len + 1);
    strncpy((*paths)[*count], start, len);
    (*paths)[*count][len] = '\0';
    
    (*count)++;
}

/**
 * 세미콜론으로 구분된 경로들을 파싱
 */
static void parse_vulnerability_paths(const char *details, char ***out_paths, size_t *out_count) {
    *out_paths = malloc(INITIAL_CAPACITY * sizeof(char *));
    *out_count = 0;
    size_t capacity = INITIAL_CAPACITY;
    
    char *copy = strdup(details);
    char *saveptr = NULL;
    const char *token = strtok_r(copy, ";", &saveptr);
    
    while (token != NULL) {
        add_vulnerability_path(out_paths, out_count, &capacity, token);
        token = strtok_r(NULL, ";", &saveptr);
    }
    
    free(copy);
}

/**
 * 파싱된 취약점 정보 하나 생성
 */
static VulnerableTarget* create_target(const char *binary_path) {
    VulnerableTarget *target = malloc(sizeof(VulnerableTarget));
    target->binary_path = strdup(binary_path);
    target->vuln_type = VULN_NONE;
    target->rpath_vulns = NULL;
    target->rpath_count = 0;
    target->weak_dylib_vulns = NULL;
    target->weak_dylib_count = 0;
    return target;
}

/**
 * Scanner 결과 파일 파싱
 */
ParsedResults* parse_scanner_output(const char *output_file) {
    FILE *fp = fopen(output_file, "r");
    if (!fp) {
        fprintf(stderr, "[ERROR] 파일을 열 수 없음: %s\n", output_file);
        return NULL;
    }
    
    ParsedResults *results = malloc(sizeof(ParsedResults));
    results->targets = malloc(INITIAL_CAPACITY * sizeof(VulnerableTarget *));
    results->target_count = 0;
    size_t capacity = INITIAL_CAPACITY;
    
    char line[MAX_LINE_LENGTH];
    VulnerableTarget *current_target = NULL;
    bool in_vulnerability_section = false;
    
    while (fgets(line, sizeof(line), fp)) {
        // 프로세스 분석 섹션 감지
        if (strstr(line, "프로세스:") && strstr(line, " (PID:")) {
            // 새로운 프로세스 발견
            if (current_target != NULL) {
                // 이전 타겟이 취약점을 가지는지 여부 확인
                if (current_target->vuln_type == VULN_NONE) {
                    free(current_target->binary_path);
                    free(current_target);
                }
            }
            
            // 경로 추출 (예: "경로: /path/to/binary")
            current_target = NULL;
            in_vulnerability_section = false;
            continue;
        }
        
        // 경로 부분 파싱 (매우 긴 경로도 처리)
        if (current_target == NULL && strstr(line, "경로:")) {
            char *path_start = strchr(line, ':') + 1;
            while (isspace(*path_start)) path_start++;
            
            // 경로의 끝을 찾기 (줄의 끝 또는 상태: 전까지)
            char *path_end = path_start + strlen(path_start) - 1;
            
            // 줄의 마지막부터 상태까지 역으로 탐색
            while (path_end > path_start && isspace(*path_end)) path_end--;
            
            // "상태:" 텍스트가 있으면제거
            char *status_marker = strstr(path_start, "상태:");
            if (status_marker && status_marker < path_end) {
                path_end = status_marker - 1;
                while (path_end > path_start && isspace(*path_end)) path_end--;
            }
            
            size_t len = path_end - path_start + 1;
            if (len > 0 && len < 4096) {
                // 동적 메모리로 긴 경로도 처리 가능
                char *path = malloc(len + 1);
                strncpy(path, path_start, len);
                path[len] = '\0';
                
                current_target = create_target(path);
                free(path);
            }
            continue;
        }
        
        // 이상 상태 감지
        if (current_target != NULL && strstr(line, "이상 감지")) {
            in_vulnerability_section = true;
            continue;
        }
        
        // RPATH 취약점 파싱
        if (current_target != NULL && in_vulnerability_section && strstr(line, "RPATH 취약 경로")) {
            // 다음 줄이 "상세:" 부분
            if (fgets(line, sizeof(line), fp) && strstr(line, "상세:")) {
                char *details_start = strchr(line, ':') + 1;
                
                // 상세 정보가 여러 줄일 수 있으므로 처리
                char details[4096] = "";
                strncpy(details, details_start, sizeof(details) - 1);
                details[sizeof(details) - 1] = '\0';
                
                // 다음 줄이 상세 정보의 연속인지 확인
                long pos = ftell(fp);
                while (fgets(line, sizeof(line), fp)) {
                    // "문제명:", "바이너리:", 또는 새로운 섹션이 시작되면 멈춤
                    if (strstr(line, "문제명:") || strstr(line, "바이너리:") || 
                        strstr(line, "상태:") || strstr(line, "━━━━━━━━━")) {
                        fseek(fp, pos, SEEK_SET);
                        break;
                    }
                    
                    // 내용 추가 (buffer overflow 방지)
                    if (strlen(line) > 2 && !isdigit(line[0])) {
                        size_t remaining = sizeof(details) - 1 - strlen(details);
                        if (remaining > 0) {
                            strncat(details, line, remaining);
                            details[sizeof(details) - 1] = '\0';
                        }
                    }
                    pos = ftell(fp);
                }
                
                parse_vulnerability_paths(details, &current_target->rpath_vulns, &current_target->rpath_count);
                current_target->vuln_type |= VULN_RPATH;
            }
            continue;
        }
        
        // Weak Dylib 취약점 파싱
        if (current_target != NULL && in_vulnerability_section && strstr(line, "Weak Dylib 취약 경로")) {
            if (fgets(line, sizeof(line), fp) && strstr(line, "상세:")) {
                char *details_start = strchr(line, ':') + 1;
                char details[4096] = "";
                strncpy(details, details_start, sizeof(details) - 1);
                details[sizeof(details) - 1] = '\0';
                
                long pos = ftell(fp);
                while (fgets(line, sizeof(line), fp)) {
                    if (strstr(line, "문제명:") || strstr(line, "바이너리:") || 
                        strstr(line, "상태:") || strstr(line, "━━━━━━━━━")) {
                        fseek(fp, pos, SEEK_SET);
                        break;
                    }
                    
                    if (strlen(line) > 2 && !isdigit(line[0])) {
                        size_t remaining = sizeof(details) - 1 - strlen(details);
                        if (remaining > 0) {
                            strncat(details, line, remaining);
                            details[sizeof(details) - 1] = '\0';
                        }
                    }
                    pos = ftell(fp);
                }
                
                parse_vulnerability_paths(details, &current_target->weak_dylib_vulns, &current_target->weak_dylib_count);
                current_target->vuln_type |= VULN_WEAK_DYLIB;
            }
            continue;
        }
        
        // 새로운 프로세스/대시라인 시작 시 현재 타겟 저장
        if ((strstr(line, "지금:") || strstr(line, "━━━━━━━━━")) && current_target != NULL) {
            if (current_target->vuln_type != VULN_NONE) {
                if (results->target_count >= capacity) {
                    capacity *= 2;
                    results->targets = realloc(results->targets, capacity * sizeof(VulnerableTarget *));
                }
                results->targets[results->target_count++] = current_target;
            } else {
                free(current_target->binary_path);
                free(current_target);
            }
            current_target = NULL;
            in_vulnerability_section = false;
        }
    }
    
    // 마지막 타겟 처리
    if (current_target != NULL && current_target->vuln_type != VULN_NONE) {
        if (results->target_count >= capacity) {
            capacity *= 2;
            results->targets = realloc(results->targets, capacity * sizeof(VulnerableTarget *));
        }
        results->targets[results->target_count++] = current_target;
    } else if (current_target != NULL) {
        free(current_target->binary_path);
        free(current_target);
    }
    
    fclose(fp);
    return results;
}

/**
 * 파싱된 결과 해제
 */
void free_parsed_results(ParsedResults *results) {
    if (!results) return;
    
    for (size_t i = 0; i < results->target_count; i++) {
        VulnerableTarget *target = results->targets[i];
        
        free(target->binary_path);
        
        for (size_t j = 0; j < target->rpath_count; j++) {
            free(target->rpath_vulns[j]);
        }
        free(target->rpath_vulns);
        
        for (size_t j = 0; j < target->weak_dylib_count; j++) {
            free(target->weak_dylib_vulns[j]);
        }
        free(target->weak_dylib_vulns);
        
        free(target);
    }
    
    free(results->targets);
    free(results);
}

/**
 * 파싱된 결과 출력
 */
void print_parsed_results(ParsedResults *results) {
    if (!results) return;
    
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("               파싱된 취약한 바이너리 정보 (로더용)\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    printf("발견된 취약한 바이너리: %zu개\n\n", results->target_count);
    
    for (size_t i = 0; i < results->target_count; i++) {
        VulnerableTarget *target = results->targets[i];
        printf("[%zu] %s\n", i + 1, target->binary_path);
        printf("    취약점 타입: ");
        
        if (target->vuln_type == VULN_RPATH) {
            printf("RPATH만\n");
        } else if (target->vuln_type == VULN_WEAK_DYLIB) {
            printf("Weak Dylib만\n");
        } else if (target->vuln_type == VULN_BOTH) {
            printf("RPATH + Weak Dylib\n");
        }
        
        if (target->rpath_count > 0) {
            printf("    [RPATH 취약 경로 (%zu개)]\n", target->rpath_count);
            for (size_t j = 0; j < target->rpath_count; j++) {
                printf("      - %s\n", target->rpath_vulns[j]);
            }
        }
        
        if (target->weak_dylib_count > 0) {
            printf("    [Weak Dylib 취약 경로 (%zu개)]\n", target->weak_dylib_count);
            for (size_t j = 0; j < target->weak_dylib_count; j++) {
                printf("      - %s\n", target->weak_dylib_vulns[j]);
            }
        }
        printf("\n");
    }
}
