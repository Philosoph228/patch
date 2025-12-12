// patcher.c - Minimal unified diff patcher for Windows (C99 + WinAPI only)
// Supports only basic unified diffs (-u or -urN) with hunk replacements
// Limitations: No file creation/removal, no fuzzy matching, no context verification

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csw.h"

#include "patch.h"

#ifdef _WIN32
#include <io.h>
#endif

#define MAX_LINE 4096
#define MAX_PATH_LEN 260

typedef struct patch_options {
    unsigned int inplace : 1;
    unsigned int apply_dates : 1;
    unsigned int verbose : 1;
} patch_options_t;

typedef struct patch_instance_data {
    patch_options_t options;
    int (*path_cbk)(char* str, stream_wrapper_t* stream);
} patch_instance_data_t;

int default_path_cbk(char* path, stream_wrapper_t* sw) {
    FILE* fp = fopen(path, "ab+");
    if (!fp) {
        return -1;
    }
    return make_fdsw(sw, fp);
}

char* sw_fgets(stream_wrapper_t* sw, char* line, int maxlen) {
    if (!line || maxlen <= 1 || !sw)
        return NULL;

    size_t i = 0;

    while (i + 1 < (size_t)maxlen) {

        /* Read one byte */
        char ch;
        int stat = sw->read(sw, &ch, 1, 1);

        if (stat < 0) {
            /* Read error → NULL (like fgets) */
            return NULL;
        }
        if (stat == 0) {
            /* EOF */
            if (i == 0)
                return NULL; /* No data → NULL */
            break;           /* Partial line OK */
        }

        line[i++] = ch;

        /* Normal newline */
        if (ch == '\n')
            break;

        if (ch == '\r') {
            /* possible CRLF */
            char next;
            stat = sw->read(sw, &next, 1, 1);

            if (stat == 1) {
                if (next == '\n') {
                    if (i + 1 < (size_t)maxlen)
                        line[i++] = next; /* include LF */
                } else {
                    /* not LF → push back */
                    sw->seekg(sw, -1, SEEK_CUR);
                }
            }

            break;
        }
    }

    line[i] = '\0';
    return line;
}

void trim_newline(char* line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = 0;
    }
}

/* finalize currently open output: copy remainder (line-by-line) if both files open,
 * close handles, move temp into place. Return 0 on success, non-zero on error.
 */
int finalize_file(FILE** inptr, FILE** outptr, char* cur_output, char* temp_path, const patch_options_t* options) {
    /* If both input and output are open, copy remaining lines from input into output. */
    if (inptr && *inptr && outptr && *outptr) {
        char buf[MAX_LINE];
        while (fgets(buf, sizeof(buf), *inptr)) {
            if (fputs(buf, *outptr) == EOF) {
                perror("Write error while copying remainder");
                /* cleanup and remove temp */
                fclose(*outptr);
                *outptr = NULL;
                fclose(*inptr);
                *inptr = NULL;
                DeleteFileA(temp_path);
                return 1;
            }
        }
        if (ferror(*inptr)) {
            perror("Read error while copying remainder");
            fclose(*outptr);
            *outptr = NULL;
            fclose(*inptr);
            *inptr = NULL;
            DeleteFileA(temp_path);
            return 1;
        }

        /* flush output to disk */
        fflush(*outptr);
#ifdef _WIN32
        _commit(_fileno(*outptr));
#endif
    }

    /* Close input if open */
    if (inptr && *inptr) {
        fclose(*inptr);
        *inptr = NULL;
    }

    /* If output is open, close and move temp into place */
    if (outptr && *outptr) {
        fclose(*outptr);
        *outptr = NULL;
        /* replace destination (ignore DeleteFileA errors) */
        DeleteFileA(cur_output);
        if (!MoveFileA(temp_path, cur_output)) {
            fprintf(stderr, "Failed to move temp '%s' -> '%s' (err %lu)\n", temp_path, cur_output, GetLastError());
            DeleteFileA(temp_path);
            return 1;
        }
    }

    return 0;
}

/* parse_header_filename:
 *  p: pointer to the text after '--- ' or '+++ '
 *  out_fname: buffer to receive filename (may be NULL)
 *  out_len: length of out_fname (may be 0)
 *
 * Returns: pointer within p just after the parsed filename (i.e. at the separator or NUL)
 */
static const char* parse_header_filename(const char* p, char* out_fname, size_t out_len) {
    /* skip spaces */
    while (*p == ' ' || *p == '\t')
        ++p;

    char buf[MAX_PATH_LEN];
    size_t bi = 0;
    int in_quote = 0;

    while (*p && *p != '\r' && *p != '\n' && bi + 1 < sizeof(buf)) {
        if (*p == '"') {
            in_quote = !in_quote;
            ++p;
            continue;
        }

        if (!in_quote && (*p == ' ' || *p == '\t'))
            break;

        if (*p == '\\' && *(p + 1)) {
            buf[bi++] = *(p + 1);
            p += 2;
            continue;
        }

        buf[bi++] = *p++;
    }

    buf[bi] = '\0';

    if (out_fname && out_len > 0) {
        if (out_len == 1) {
            out_fname[0] = '\0';
        } else {
            strncpy(out_fname, buf, out_len - 1);
            out_fname[out_len - 1] = '\0';
        }
    }

    /* p now points at separator (space/tab/newline) or end -- return that */ 
    return p;
}

int apply_patch(void* self, stream_wrapper_t* sw) {
    if (self == NULL)   /* Invalid instance pointer */
        return 1;
    patch_instance_data_t* instance = (patch_instance_data_t*)self;

    /* shorthand */
    patch_options_t* options = &instance->options;

    if (sw == NULL) {
        fprintf(stderr, "Invalid stream handle");
        return 1;
    }
    if (options->verbose)
        printf("Opened patch\n");

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
            if (!sw_fgets(sw, line, MAX_LINE))
                break;
        }

        /* Note: lines read from patch may contain CRLF; trim_newline when parsing filenames later */
        /* Trim newline for header parsing convenience */
        char line_copy[MAX_LINE];
        strncpy(line_copy, line, MAX_LINE);
        trim_newline(line_copy);

        /* Note: lines read from patch may contain CRLF; trim_newline when parsing filenames later */
        if (strncmp(line_copy, "--- ", 4) == 0) {
            /* When starting a new diff, if we have currently open input/output finalize it first. */

            if (input || output) {
                if (options->verbose)
                    printf("Finalizing the previous file: %s\n", new_file);
                if (finalize_file(&input, &output, new_file, temp_path, options) != 0) {
                    sw->close(sw);
                    return 1;
                }
                /* reset filenames/timestamp */
                *orig_file = *new_file = '\0';
            }
            /* parse original filename (token after '--- ') */
            const char* after = parse_header_filename(line_copy + 4, orig_file, sizeof(orig_file));
            if (options->verbose)
                printf("Found orig: '%s'\n", orig_file);
        } else if (strncmp(line_copy, "+++ ", 4) == 0) {
            /* parse new filename */
            const char* after = parse_header_filename(line_copy + 4, new_file, sizeof(new_file));
            if (options->verbose)
                printf("Found new: '%s'\n", new_file);
            /* the rest of the line may be a timestamp. */

            /* At this point we have both orig_file and new_file (or at least new_file). Open input and output */
            if (input) {
                fclose(input);
                input = NULL;
            }
            if (output) {
                fclose(output);
                output = NULL;
            }

            /* Determine where to read and where to write based on  */
            int write_inplace = strcmp(orig_file, new_file) == 0;
            const char* read_path = orig_file[0] ? orig_file : new_file;    /* fallback */
            const char* write_path = write_inplace ? orig_file : new_file;

            /* Open the target file in binary mode to preserve bytes */
            input = fopen(read_path, "rb");
            if (!input) {
                fprintf(stderr, "Cannot open target file: %s\n", read_path);
                sw->close(sw);
                return 1;
            }

            /* create temp path based on write_path */
            snprintf(temp_path, sizeof(temp_path), "%s.tmp", write_path);
            temp_path[sizeof(temp_path) - 1] = '\0';
            output = fopen(temp_path, "wb");
            if (!output) {
                fprintf(stderr, "Cannot create temp file: %s\n", temp_path);
                fclose(input);
                sw->close(sw);
                return 1;
            }

            /* reset current input line tracking for this file */
            cur_input_line = 1;
        } else if (strncmp(line, "@@ ", 3) == 0) {
            /* hunk header line */
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
                sw->close(sw);
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
                if(!sw_fgets(sw, line, MAX_LINE)) {
                    /* EOF while inside hunk; break out */
                    break;
                }

                /* If this line looks like the start of a file header or next hunk,
                   push it back and stop processing the current hunk.
                   This prevents lines like "+++ new.cmd ..." or "--- old.cmd ..." from
                   being treated as hunk content. */
                if (strncmp(line, "+++ ", 4) == 0 || strncmp(line, "--- ", 4) == 0 || strncmp(line, "@@ ", 3) == 0) {
                    strncpy(pushback, line, MAX_LINE);
                    pushback[MAX_LINE - 1] = '\0';
                    has_pushback = 1;
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
                    /* Normally shouldn't get here */
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

    /* after loop, finalize any remaining open file */
    if (input || output) {
        if (options->verbose)
            printf("Finalizing last file: %s\n", new_file);
        if (finalize_file(&input, &output, new_file, temp_path, options) != 0) {
            sw->close(sw);
            return 1;
        }
    }

    sw->close(sw);
    return 0;
}

void* patch_init() {
    patch_instance_data_t* instance = calloc(1, sizeof(patch_instance_data_t));
    return instance;
}

int patch_destroy(void* self) {
    if (self == NULL) /* Invalid instance pointer */
        return -1;
    patch_instance_data_t* instance = (patch_instance_data_t*)self;

    free(self);
    return 0;
}

int patch_set_options(void* self, unsigned int opts) {
    if (self == NULL)   /* Invalid instance pointer */
        return -1;
    patch_instance_data_t* instance = (patch_instance_data_t*)self;

    if (opts & PATCH_OPTION_INPLACE) {
        instance->options.inplace = 1;
    }
    if (opts & PATCH_OPTION_APPLYDATES) {
        instance->options.apply_dates = 1;
    }
    if (opts & PATCH_OPTION_VERBOSE) {
        instance->options.verbose = 1;
    }

    return 1;
}
