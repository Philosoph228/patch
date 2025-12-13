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
    patch_event_cbk_t* path_cbk;
    void* path_cbk_userdata;
} patch_instance_data_t;

/* private */
int patch_call_user_cbk(patch_instance_data_t* instance, patch_evt_t* evt) {
    if (instance == NULL)   /* Invalid instance pointer */
        return -1;
    evt->userdata = instance->path_cbk_userdata;
    return instance->path_cbk(evt);
}

/* private */
int patch_acquire_user_stream(patch_instance_data_t* instance, char* path, stream_wrapper_t* sw_ptr, unsigned int purpose) {
    patch_evt_t event = { 0 };
    event.type = PATCH_EVT_STREAM_ACQUIRE;
    event.data.stream_event.path = path;
    event.data.stream_event.stream = sw_ptr;
    event.data.stream_event.purpose = purpose;
    return patch_call_user_cbk(instance, &event);
}

/* private */
int patch_release_user_stream(patch_instance_data_t* instance, char* path, stream_wrapper_t* sw_ptr, unsigned int purpose) {
    patch_evt_t event = { 0 };
    event.type = PATCH_EVT_STREAM_RELEASE;
    event.data.stream_event.path = path;
    event.data.stream_event.stream = sw_ptr;
    event.data.stream_event.purpose = purpose;
    return patch_call_user_cbk(instance, &event);
}

int default_patch_evt_cbk(patch_evt_t* evt) {
    if (evt == NULL) /* Invalid evt */
        return -1;

    if (evt->type == PATCH_EVT_STREAM_ACQUIRE || evt->type == PATCH_EVT_STREAM_RELEASE) {
        char* path = evt->data.stream_event.path;
        stream_wrapper_t* sw = evt->data.stream_event.stream;
        unsigned int purpose = evt->data.stream_event.purpose;

        if (sw == NULL) /* invalid stream wrapper provided */
            return -1;

        char actual_path[MAX_PATH] = {0};
        strcpy(actual_path, path);
        /* If it is output stream, add .tmp extension to create a temporary file */
        if (purpose == PATCH_STREAM_PURPOSE_OUTPUT) {
            snprintf(actual_path, sizeof(actual_path), "%s.tmp", path);
        }
        actual_path[sizeof(actual_path) - 1] = '\0';

        switch (evt->type) {
        case PATCH_EVT_STREAM_ACQUIRE: {
            /* create a new file stream from path */
            FILE* fp = fopen(actual_path, purpose == PATCH_STREAM_PURPOSE_OUTPUT ? "wb" : "rb");
            if (!fp) {  /* Cannot open the file at specified path */
                return -1;
            }
            return make_fdsw(sw, fp);
        }
            break;
        case PATCH_EVT_STREAM_RELEASE: {
            /* close the file stream at release request */

            if (purpose == PATCH_STREAM_PURPOSE_OUTPUT) {
                sw->close(sw);  /* first close the tmp file */
                /* move temp into place */
                DeleteFileA(path); /* delete already existing target file to avoid errors */
                if (!MoveFileA(actual_path, path)) {
                    fprintf(stderr, "Failed to move temp '%s' -> '%s' (err %lu)\n", actual_path, path, GetLastError());
                    DeleteFileA(actual_path);
                    return -1;
                }
            }

            return sw->close(sw);
        }
            break;
        /* default falls to function return */
        }
    }

    return -1;  /* Unknown event, return error */
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

int sw_fputs(stream_wrapper_t* sw, const char* s) {
    if (!sw || !s)
        return -1; /* error */

    size_t len = 0;

    /* Compute length of string */
    while (s[len] != '\0') {
        len++;
    }

    if (len == 0)
        return 0; /* nothing to write */

    /* Write to stream */
    long written = sw->write(sw, s, 1, len);

    if (written < 0 || (size_t)written != len)
        return -1; /* write error */

    return (int)len; /* return number of characters written */
}

void trim_newline(char* line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = 0;
    }
}

/* finalize currently open output: copy remainder (line-by-line) if both files open
 * requests the user to unref streams
 * Return 0 on success, non-zero on error.
 */
int finalize_file(patch_instance_data_t* instance, stream_wrapper_t* in_stream, stream_wrapper_t* out_stream, char* in_path, char* out_path) {
    if (instance == NULL)   /* Invalid instance pointer */
        return 0;

    /* If both input and output are open, copy remaining lines from input into output. */
    if ((in_stream && in_stream->_impl) && (out_stream && out_stream->_impl)) {
        char buf[MAX_LINE];
        while (sw_fgets(in_stream, buf, sizeof(buf))) {
            if (sw_fputs(out_stream, buf) <= 0) {
                perror("Write error while copying remainder");
                /* cleanup and remove temp */
                patch_release_user_stream(instance, out_path, out_stream, PATCH_STREAM_PURPOSE_OUTPUT);
                memset(out_stream, 0, sizeof(stream_wrapper_t));
                patch_release_user_stream(instance, in_path, in_stream, PATCH_STREAM_PURPOSE_INPUT);
                memset (in_stream, 0, sizeof(stream_wrapper_t));
                return 1;
            }
        }
        /* TODO(csw):
        if (ferror(*inptr)) {
            perror("Read error while copying remainder");
            fclose(*outptr);
            *outptr = NULL;
            fclose(*inptr);
            *inptr = NULL;
            DeleteFileA(temp_path);
            return 1;
        }
        */

        /* flush output to disk */
        /* TODO(csw)?
        fflush(*outptr);
#ifdef _WIN32
        _commit(_fileno(*outptr));
#endif
        */
    }

    /* Close input if open */
    if (in_stream && in_stream->_impl) {
        patch_release_user_stream(instance, in_path, in_stream, PATCH_STREAM_PURPOSE_INPUT);
        memset(in_stream, 0, sizeof(stream_wrapper_t));
    }

    /* Close output if open */
    if (out_stream && out_stream->_impl) {
        patch_release_user_stream(instance, out_path, out_stream, PATCH_STREAM_PURPOSE_OUTPUT);
        memset(out_stream, 0, sizeof(stream_wrapper_t));
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
    stream_wrapper_t output_stream = { 0 };
    stream_wrapper_t input_stream = { 0 };

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

            if (input_stream._impl || output_stream._impl) {
                if (options->verbose)
                    printf("Finalizing the previous file: %s\n", new_file);
                if (finalize_file(instance, &input_stream, &output_stream, orig_file, new_file) != 0) {
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
            if (input_stream._impl) {
                input_stream.close(&input_stream);
                memset(&input_stream, 0, sizeof(stream_wrapper_t));
            }
            if (output_stream._impl) {
                output_stream.close(&output_stream);
                memset(&output_stream, 0, sizeof(stream_wrapper_t));
            }

            /* Determine where to read and where to write based on  */
            int write_inplace = strcmp(orig_file, new_file) == 0;
            const char* read_path = orig_file[0] ? orig_file : new_file;    /* fallback */
            const char* write_path = write_inplace ? orig_file : new_file;

            /* Open the target file in binary mode to preserve bytes */
            int stat = patch_acquire_user_stream(instance, read_path, &input_stream, PATCH_STREAM_PURPOSE_INPUT);
            if (stat != 0) {
                fprintf(stderr, "Cannot open source file: %s\n", read_path);
                sw->close(sw);
                return 1;
            }

            /* create temp path based on write_path */
            stat = patch_acquire_user_stream(instance, write_path, &output_stream, PATCH_STREAM_PURPOSE_OUTPUT);
            if (stat != 0) {
                fprintf(stderr, "Cannot create resulted patched file: %s\n", write_path);
                sw->close(sw);
                return 1;
            }

            /* reset current input line tracking for this file */
            cur_input_line = 1;
        } else if (strncmp(line, "@@ ", 3) == 0) {
            /* hunk header line */
            int start_old = 0, len_old = 0, start_new = 0, len_new = 0;
            int parsed = sscanf(line, "@@ -%d,%d +%d,%d @@", &start_old, &len_old, &start_new, &len_new);
            if (parsed != 4) {
                fprintf(stderr, "Malformed or unsupported hunk header (counts required): %s\n", line);
                sw->close(sw);
                return 1;
            }

            if (!input_stream._impl || !output_stream._impl) {
                fprintf(stderr, "Hunk encountered but no file opened for patching.\n");
                sw->close(sw);
                return 1;
            }

            /* Write lines from input up to start_old - 1, but only the delta from current position */
            char file_line[MAX_LINE];
            int i;
            for (i = cur_input_line; i < start_old; ++i) {
                if (!sw_fgets(&input_stream, file_line, MAX_LINE))
                    break;
                if (sw_fputs(&output_stream, file_line) <= 0) {
                    fprintf(stderr, "Write error while copying pre-hunk lines");
                    sw->close(sw);
                    return 1;
                }
                ++cur_input_line;
            }

            /* Process hunk lines; Strictly track numbers of old/new lines processed
             * from the @@ header (len_old, len_new). Treat any line that does not
             * start with ' ', '+' or '-' as the start of the next header and push
             * it back. If counts are not satisfied when the hunk ends, treat it
             * as an error (malformed patch). */
            int proc_old = 0;   /* how many old (input) lines consumed in this hunk */
            int proc_new = 0;   /* how many new (output) lines consumed in this hunk */

            while (proc_old < len_old || proc_new < len_new) {
                if (!sw_fgets(sw, line, MAX_LINE)) {
                    fprintf(stderr, "Unexpected EOF inside hunk header at file '%s'.\n", new_file);
                    sw->close(sw);
                    return 1;
                }

                /* Non-hunk-leading char → push back as next header/start and stop. */
                if (line[0] != ' ' && line[0] != '+' && line[0] != '-') {
                    strncpy(pushback, line, MAX_LINE);
                    pushback[MAX_LINE - 1] = '\0';
                    has_pushback = 1;
                    break;
                }

                if (line[0] == '-') {
                    /* deleted line: consume one line from input but do not write it */
                    if (sw_fgets(&input_stream, file_line, MAX_LINE)) {
                        ++cur_input_line;
                        ++proc_old;
                    } else {
                        /* unexpected EOF in input; count is as consumed for strictness */
                        ++proc_old;
                    }
                } else if (line[0] == '+') {
                    /* added line: write without consuming input; write everything after the '+' */
                    if (sw_fputs(&output_stream, line + 1) <= 0) {
                        fprintf(stderr, "Write error while applying hunk");
                        sw->close(sw);
                        return 1;
                    }
                    ++proc_new;
                } else {    /* line[0] == ' ' */
                    /* context line: copy from input to output */
                    if (sw_fgets(&input_stream, file_line, MAX_LINE)) {
                        if (sw_fputs(&output_stream, file_line) <= 0) {
                            fprintf(stderr, "Write error while applying hunk");
                            sw->close(sw);
                            return 1;
                        }
                        ++cur_input_line;
                        ++proc_old;
                        ++proc_new;
                    } else {
                        /* unexpected EOF in input; write the provided context instead */
                        if (sw_fputs(&output_stream, line + 1) <= 0) {
                            fprintf(stderr, "Write error while applying hunk");
                            sw->close(sw);
                            return 1;
                        }
                        ++proc_new;
                    }
                }
            }

            /* continue to outer loop which will consume the pushback if set */
        }
        /* other lines in patch are ignored (e.g., index lines, timestamps) */
    }

    /* after loop, finalize any remaining open file */
    if (input_stream._impl || output_stream._impl) {
        if (options->verbose)
            printf("Finalizing last file: %s\n", new_file);
        if (finalize_file(instance, &input_stream, &output_stream, orig_file, new_file) != 0) {
            sw->close(sw);
            return 1;
        }
    }

    sw->close(sw);
    return 0;
}

void* patch_init() {
    patch_instance_data_t* instance = calloc(1, sizeof(patch_instance_data_t));

    instance->path_cbk = &default_patch_evt_cbk;

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

int patch_set_path_cbk(void* self, patch_event_cbk_t* new_cbk, void* userdata) {
    if (self == NULL) /* Invalid instance pointer */
        return -1;
    patch_instance_data_t* instance = (patch_instance_data_t*)self;

    instance->path_cbk = new_cbk;
    instance->path_cbk_userdata = userdata;

    return 0;
}
