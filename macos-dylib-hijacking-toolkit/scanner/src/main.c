#include "process_scanner.h"
#include "bundle_scanner.h"
#include "scan_report.h"
#include "options.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\n");
    printf("Scan Modes (choose one):\n");
    printf("  -b <path>      Scan .app bundles under <path> (static, pre-deployment)\n");
    printf("  (none)         Scan all running processes (runtime, default)\n");
    printf("\n");
    printf("Common Options:\n");
    printf("  -v             Verbose output (debug info)\n");
    printf("  -q             Quiet (default)\n");
    printf("  -o <file>      Write report to <file> (default: payload/report.txt)\n");
    printf("  --stdout       Write report to stdout instead of file\n");
    printf("  -h, --help     Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                          # scan running processes\n", prog);
    printf("  %s -b ./eval-apps           # scan .app bundles in ./eval-apps\n", prog);
    printf("  %s -b ./eval-apps -o report.txt -v\n", prog);
}

int main(int argc, char **argv) {
    // 기본 설정
    g_verbose = 0;
    const char *output_file = "payload/report.txt";
    const char *bundle_dir = NULL;
    bool mode_specified = false;

    // 옵션 파싱
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0) {
            if (mode_specified) {
                fprintf(stderr, "[ERROR] 스캔 모드는 하나만 지정 가능합니다 (-b 또는 기본값).\n");
                return 1;
            }
            if (i + 1 < argc) {
                bundle_dir = argv[++i];
                mode_specified = true;
            } else {
                fprintf(stderr, "[ERROR] -b 옵션에는 디렉터리 경로가 필요합니다.\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-v") == 0) {
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
            output_file = NULL;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "[ERROR] 알 수 없는 옵션: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // 출력 리다이렉션 (파일 지정 시)
    if (output_file) {
        FILE *out = fopen(output_file, "w");
        if (!out) {
            fprintf(stderr, "[ERROR] 출력 파일을 열 수 없음: %s\n", output_file);
            return 1;
        }
        dup2(fileno(out), STDOUT_FILENO);
        fclose(out);
    }

    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                   macOS Dylib Injection Scanner                    \n");
    printf("════════════════════════════════════════════════════════════════════\n");

    ScanReport *results = NULL;   // ← 타입 변경

    if (bundle_dir) {
        printf("[*] 번들 디렉터리 스캔 시작: %s\n\n", bundle_dir);
        results = scan_bundle_directory(bundle_dir, g_verbose);
        // 번들 모드: 프로세스 정보 없음
        if (results) scan_report_print(results, false);
    } else {
        printf("[*] 프로세스 스캔 시작...\n\n");
        results = scan_all_processes(g_verbose);   // ← 반환 타입 ScanReport *
        // 프로세스 모드: 프로세스 정보 출력
        if (results) scan_report_print(results, true);
    }

    if (!results) {
        printf("[!] 스캔 결과가 없습니다.\n");
    }

    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                         스캔 완료                                  \n");
    printf("════════════════════════════════════════════════════════════════════\n");

    if (output_file) {
        fprintf(stderr, "[✓] 결과 저장: %s\n", output_file);
    }

    if (results) scan_report_free(results);   // ← 해제 함수 변경
    return 0;
}