#include "process_scanner.h"
#include "options.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("  -v             verbose (show debug)\n");
    printf("  -q             quiet (default)\n");
    printf("  -o <file>      output to specific file\n");
    printf("  --stdout       output to stdout (instead of file)\n");
    printf("  -h, --help     show this help message\n\n");
    printf("Default: results saved to payload/report.txt\n");
}

int main(int argc, char **argv) {
    // 기본 설정
    g_verbose = 0;
    const char *output_file = "payload/report.txt";  // 기본값 (프로젝트 루트 기준)
    
    // 옵션 파싱
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            g_verbose = 1;
        } else if (strcmp(argv[i], "-q") == 0) {
            g_verbose = 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "[ERROR] -o 옵션에는 파일명이 필요합니다.\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--stdout") == 0) {
            output_file = NULL;  // stdout으로 출력
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    // 출력 리다이렉션 (파일 지정 시)
    FILE *original_stdout = NULL;
    if (output_file) {
        FILE *out = fopen(output_file, "w");
        if (!out) {
            fprintf(stderr, "[ERROR] 출력 파일을 열 수 없음: %s\n", output_file);
            return 1;
        }
        // stdout을 파일로 리다이렉트
        dup2(fileno(out), STDOUT_FILENO);
        fclose(out);
    }

    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                   macOS Dylib Injection Scanner                    \n");
    printf("                  (Spectre Vulnerability Detection)                 \n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("[*] 프로세스 스캔 시작...\n\n");

    ProcessReportArray *results = scan_all_processes();

    if (results) {
        process_report_array_print(results, false);
        process_report_array_free(results);
    }

    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                         스캔 완료                                  \n");
    printf("════════════════════════════════════════════════════════════════════\n");
    
    // stderr로 저장 위치 알림
    if (output_file) {
        fprintf(stderr, "[✓] 결과 저장: %s\n", output_file);
    }

    return 0;
}