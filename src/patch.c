// patcher.c - Minimal unified diff patcher for Windows (C99 + WinAPI only)
// Supports only basic unified diffs (-u or -urN) with hunk replacements
// Limitations: No file creation/removal, no fuzzy matching, no context verification

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4096
#define MAX_PATH_LEN 260

typedef struct patch_options {
    unsigned int inplace : 1;
    unsigned int apply_dates : 1;
    unsigned int verbose : 1;
} patch_options_t;

void trim_newline(char* line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = 0;
    }
}

int apply_patch(const char* patch_file, const patch_options_t* options) {
    /* Open patch file in binary mode to avoid text-mode newline translations */
    FILE* patch = fopen(patch_file, "rb");
    if (!patch) {
        fprintf(stderr, "Cannot open patch file: %s\n", patch_file);
        return 1;
    }

    char line[MAX_LINE];
    char pushback[MAX_LINE];
    int has_pushback = 0;

    char orig_file[MAX_PATH_LEN] = {0};
    char new_file[MAX_PATH_LEN] = {0};
    FILE* output = NULL;
    FILE* input = NULL;
    char temp_path[MAX_PATH_LEN];

    int cur_input_line = 1; /* track current line number in input file (1-based) */

    for (;;) {
        /* Outer loop: prefer pushback line if available, else read from patch */
        if (has_pushback) {
            /* copy pushback into line and clear the flag */
            strncpy(line, pushback, MAX_LINE);
            line[MAX_LINE - 1] = '\0';
            has_pushback = 0;
        } else {
            if (!fgets(line, MAX_LINE, patch))
                break;
        }

        /* Note: lines read from patch may contain CRLF; trim_newline when parsing filenames later */
        if (strncmp(line, "--- ", 4) == 0) {
            sscanf(line + 4, "%s", orig_file);
            trim_newline(orig_file);
        } else if (strncmp(line, "+++ ", 4) == 0) {
            sscanf(line + 4, "%s", new_file);
            trim_newline(new_file);

            if (input) {
                fclose(input);
                input = NULL;
            }
            if (output) {
                fclose(output);
                output = NULL;
            }

            /* Open the target file in binary mode to preserve bytes */
            input = fopen(new_file, "rb");
            if (!input) {
                fprintf(stderr, "Cannot open target file: %s\n", new_file);
                fclose(patch);
                return 1;
            }

            /* reset current input line tracking for this file */
            cur_input_line = 1;

            snprintf(temp_path, MAX_PATH_LEN, "%s.tmp", new_file);
            output = fopen(temp_path, "wb");
            if (!output) {
                fprintf(stderr, "Cannot create temp file.\n");
                fclose(input);
                fclose(patch);
                return 1;
            }
        } else if (strncmp(line, "@@ ", 3) == 0) {
            int start_old = 0, len_old = 0, start_new = 0, len_new = 0;
            int parsed = sscanf(line, "@@ -%d,%d +%d,%d @@", &start_old, &len_old, &start_new, &len_new);
            if (parsed < 4) {
                /* try simpler forms; treat missing counts as 1 */
                parsed = sscanf(line, "@@ -%d +%d @@", &start_old, &start_new);
                if (parsed >= 1)
                    len_old = 1;
                if (parsed >= 2)
                    len_new = 1;
            }

            if (!input || !output) {
                fprintf(stderr, "Hunk encountered but no file opened for patching.\n");
                fclose(patch);
                return 1;
            }

            /* Write lines from input up to start_old - 1, but only the delta from current position */
            char file_line[MAX_LINE];
            int i;
            for (i = cur_input_line; i < start_old; ++i) {
                if (!fgets(file_line, MAX_LINE, input))
                    break;
                fputs(file_line, output);
                ++cur_input_line;
            }

            /* Process hunk lines; when a non-hunk line is read, push it back for outer loop */
            while (1) {
                if (!fgets(line, MAX_LINE, patch)) {
                    /* EOF while inside hunk; break out */
                    break;
                }

                /* In binary mode fgets will include CR if present; for hunk control characters
                   we only check the first byte which will be '-'/'+'/' ' when properly formatted. */
                if (line[0] == '-') {
                    /* deleted line: consume one line from input but do not write it */
                    if (fgets(file_line, MAX_LINE, input)) {
                        ++cur_input_line;
                    } else {
                        /* unexpected EOF in input; continue */
                    }
                } else if (line[0] == '+') {
                    /* added line: write without consuming input; write everything after the '+' */
                    fputs(line + 1, output);
                } else if (line[0] == ' ') {
                    /* context line: copy from input to output */
                    if (fgets(file_line, MAX_LINE, input)) {
                        fputs(file_line, output);
                        ++cur_input_line;
                    } else {
                        /* unexpected EOF in input; still write the context from patch */
                        fputs(line + 1, output);
                    }
                } else {
                    /* This is the start of the next hunk or next file header.
                       Push the line back so outer loop will process it (no fseek). */
                    strncpy(pushback, line, MAX_LINE);
                    pushback[MAX_LINE - 1] = '\0';
                    has_pushback = 1;
                    break;
                }
            }

            /* continue to outer loop which will consume the pushback if set */
        }
        /* other lines in patch are ignored (e.g., index lines, timestamps) */
    }

    /* Copy remaining lines from input to output */
    if (input && output) {
        char rest_line[MAX_LINE];
        while (fgets(rest_line, MAX_LINE, input)) {
            fputs(rest_line, output);
        }
    }

    if (input)
        fclose(input);
    if (output)
        fclose(output);
    if (new_file[0]) {
        /* Replace original file with temp */
        DeleteFileA(new_file);
        MoveFileA(temp_path, new_file);
    }

    fclose(patch);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <patchfile>\n", argv[0]);
        return 1;
    }

    patch_options_t options = { 0 };

    return apply_patch(argv[1], &options);
}
