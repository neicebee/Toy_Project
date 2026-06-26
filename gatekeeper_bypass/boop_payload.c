// boop_payload_exec.c
// -------------------
// Harmless native payload for macOS (or other POSIX systems).
//
// Behavior:
// - Creates ~/boop_payload_exec_test/ if it does not exist.
// - Appends a log line to exec_payload_log.txt with timestamp,
//   current working directory and a fixed message.
// - Prints to stdout where the log was written.
//
// No 네트워크, 파일 삭제, 권한 상승 없음.

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static void safe_strcpy(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;
    strncat(dst, src, dst_size - 1);
}

int main(void) {
    /* 1. 홈 디렉터리 결정 */
    const char *home = getenv("HOME");
    if (!home || !*home) {
        fprintf(stderr, "[boop_payload_exec] HOME not set\n");
        return 1;
    }

    /* 2. 로그 디렉터리: ~/boop_payload_exec_test */
    char dir_path[PATH_MAX];
    safe_strcpy(dir_path, sizeof(dir_path), home);
    strncat(dir_path, "/boop_payload_exec_test", sizeof(dir_path) - strlen(dir_path) - 1);

    /* mkdir, 이미 있으면 무시 */
    if (mkdir(dir_path, 0700) != 0 && errno != EEXIST) {
        perror("[boop_payload_exec] mkdir failed");
        return 1;
    }

    /* 3. 로그 파일 경로 */
    char log_path[PATH_MAX];
    safe_strcpy(log_path, sizeof(log_path), dir_path);
    strncat(log_path, "/exec_payload_log.txt", sizeof(log_path) - strlen(log_path) - 1);

    /* 4. 타임스탬프 / CWD 등 정보 준비 */
    time_t now = time(NULL);
    struct tm tm_now;
    char time_buf[64] = "unknown-time";

    if (localtime_r(&now, &tm_now) != NULL) {
        if (strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S%z", &tm_now) == 0) {
            safe_strcpy(time_buf, sizeof(time_buf), "strftime-failed");
        }
    }

    char cwd_buf[PATH_MAX] = "unknown-cwd";
    if (getcwd(cwd_buf, sizeof(cwd_buf)) == NULL) {
        safe_strcpy(cwd_buf, sizeof(cwd_buf), "getcwd-failed");
    }

    /* 5. 로그 한 줄 append */
    FILE *f = fopen(log_path, "a");
    if (!f) {
        perror("[boop_payload_exec] fopen failed");
        return 1;
    }

    fprintf(f, "%s\t%s\tpayload_exec executed\n", time_buf, cwd_buf);
    fclose(f);

    /* 6. stdout에 경로 출력 */
    printf("[boop_payload_exec] Log written to: %s\n", log_path);
    return 0;
}