#include <stdio.h>
#include <string.h>

#include "../../src/patch.h"

typedef struct owned_dynmem_stream {
    dynmem_t mem;
    stream_wrapper_t stream;
} owned_dynmem_stream_t;

/* virtual text file wrapper */
typedef struct vtf_wrapper {
    const char* path;
    const char* data;
    size_t length;
} vtf_wrapper_t;

typedef struct test_case_data {
    const vtf_wrapper_t* input;
    const vtf_wrapper_t* diff;
    const vtf_wrapper_t* expected;
} test_case_data_t;

typedef struct simple_test_data {
    const test_case_data_t* case_data;
    owned_dynmem_stream_t diff_owned_stream;
    owned_dynmem_stream_t infile_owned_stream;
    owned_dynmem_stream_t outfile_owned_stream;
} simple_test_data_t;

/* generated. Do not edit, use ./scripts/generate_vtf_from_file.py */
static const vtf_wrapper_t g_test_case_naughty__input = {
    .path = "./tests/data/naughty/naughty1.txt",
    .data =
        "-- old.cmd\t2025-12-10 18:46:54 +0500\r\n"
        "-- new.cmd\t2025-12-10 18:46:49 +0500\r\n"
        "@@ -1,4 +1,4 @@\r\n"
        "- REM Version 0.0.1\r\n"
        "+ REM Version 0.0.2\r\n",
    .length = 135,
};

/* generated. Do not edit, use ./scripts/generate_vtf_from_file.py */
static const vtf_wrapper_t g_test_case_naughty__diff = {
    .path = "./tests/data/naughty/diff.patch",
    .data =
        "--- ./tests/data/naughty/naughty1.txt\t2025-12-13 05:30:36 +0500\r\n"
        "+++ ./tests/data/naughty/naughty2.txt\t2025-12-13 05:30:36 +0500\r\n"
        "@@ -1,5 +1,5 @@\r\n"
        "--- old.cmd\t2025-12-10 18:46:54 +0500\r\n"
        "--- new.cmd\t2025-12-10 18:46:49 +0500\r\n"
        "+++ old.cmd\t2025-12-10 18:46:54 +0500\r\n"
        "+++ new.cmd\t2025-12-10 18:46:49 +0500\r\n"
        " @@ -1,4 +1,4 @@\r\n"
        " - REM Version 0.0.1\r\n"
        " + REM Version 0.0.2\r\n",
    .length = 365,
};

/* generated. Do not edit, use ./scripts/generate_vtf_from_file.py */
static const vtf_wrapper_t g_test_case_naugty__expected = {
    .path = "./tests/data/naughty/naughty2.txt",
    .data =
        "++ old.cmd\t2025-12-10 18:46:54 +0500\r\n"
        "++ new.cmd\t2025-12-10 18:46:49 +0500\r\n"
        "@@ -1,4 +1,4 @@\r\n"
        "- REM Version 0.0.1\r\n"
        "+ REM Version 0.0.2\r\n",
    .length = 135,
};

static const test_case_data_t g_test_case_naughty = {
    .input = &g_test_case_naughty__input,
    .diff = &g_test_case_naughty__diff,
    .expected = &g_test_case_naugty__expected, 
};

/* generated. Do not edit, use ./scripts/generate_vtf_from_file.py */
static const vtf_wrapper_t g_test_case_normal__input = {
    .path = "./tests/data/input.txt",
    .data =
        "int main() {\r\n"
        "  return 0;\r\n"
        "}\r\n"
        "\r\n",
    .length = 32,
};

/* generated. Do not edit, use ./scripts/generate_vtf_from_file.py */
static const vtf_wrapper_t g_test_case_normal__diff = {
    .path = "./tests/data/normal.diff",
    .data =
        "--- ./tests/data/input.txt\t2025-12-13 05:30:36 +0500\r\n"
        "+++ ./tests/data/output.txt\t2025-12-13 05:30:36 +0500\r\n"
        "@@ -1,4 +1,7 @@\r\n"
        "+#include <stdio.h>\r\n"
        "+\r\n"
        " int main() {\r\n"
        "+  printf(\"Hello, my world!\");\r\n"
        "   return 0;\r\n"
        " }\r\n"
        " \r\n",
    .length = 218,
};

/* generated. Do not edit, use ./scripts/generate_vtf_from_file.py */
static const vtf_wrapper_t g_test_case_normal__expected = {
    .path = "./tests/data/output.txt",
    .data =
        "#include <stdio.h>\r\n"
        "\r\n"
        "int main() {\r\n"
        "  printf(\"Hello, my world!\");\r\n"
        "  return 0;\r\n"
        "}\r\n"
        "\r\n",
    .length = 85,
};

static const test_case_data_t g_test_case_normal = {
    .input = &g_test_case_normal__input,
    .diff = &g_test_case_normal__diff,
    .expected = &g_test_case_normal__expected,
};

int test_cbk(patch_evt_t* evt) {
    if (evt == NULL) /* Invalid evt */
        return -1;
    if (evt->userdata == NULL) /* invalid userdata pointer */
        return -1;
    simple_test_data_t* dat = (simple_test_data_t*)evt->userdata;

    if (evt->type == PATCH_EVT_STREAM_ACQUIRE || evt->type == PATCH_EVT_STREAM_RELEASE) {
        char* path = evt->data.stream_event.path;
        stream_wrapper_t* sw = evt->data.stream_event.stream;

        if (sw == NULL) /* invalid stream wrapper provided */
            return -1;

        printf("Patcher requests path: %s\n", path);

        switch (evt->type) {
        case PATCH_EVT_STREAM_ACQUIRE: {
            if (strcmp(path, dat->case_data->input->path) == 0) {
                memcpy(sw, &dat->infile_owned_stream.stream, sizeof(stream_wrapper_t));
                return 0;
            }
            if (strcmp(path, dat->case_data->expected->path) == 0) {
                memcpy(sw, &dat->outfile_owned_stream.stream, sizeof(stream_wrapper_t));
                return 0;
            }
            return -1;
        } break;
        case PATCH_EVT_STREAM_RELEASE: {
            /* Do not release for now, just return success */
            return 0;
        } break;
        }
    }

    return -1; /* Unknown event, return error */
}

int init_test_context(simple_test_data_t* context_data, const test_case_data_t* case_data) {
    if (context_data == NULL)
        return -1;
    if (case_data == NULL)
        return -1;

    context_data->case_data = case_data;

    /* Populate dynmems */
    dynmem_write(&context_data->diff_owned_stream.mem, case_data->diff->data, case_data->diff->length, 1);
    dynmem_write(&context_data->infile_owned_stream.mem, case_data->input->data, case_data->input->length, 1);
    /* empty dynmem for outfile_owned_stream (will initialize itself at first write) */
    memset(&context_data->outfile_owned_stream.mem, 0, sizeof(dynmem_t));

    /* Make their streams */
    make_memsw(&context_data->diff_owned_stream.stream, &context_data->diff_owned_stream.mem);
    make_memsw(&context_data->infile_owned_stream.stream, &context_data->infile_owned_stream.mem);
    make_memsw(&context_data->outfile_owned_stream.stream, &context_data->outfile_owned_stream.mem);
}

int main() {

    test_case_data_t* test_cases[] = {
        &g_test_case_normal,
        &g_test_case_naughty,
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(*test_cases); ++i) {
        simple_test_data_t test_data = {0};

        init_test_context(&test_data, test_cases[i]);

        void* patcher = patch_init();
        patch_set_options(patcher, PATCH_OPTION_VERBOSE);
        patch_set_path_cbk(patcher, (patch_event_cbk_t*)&test_cbk, (void*)&test_data);
        apply_patch(patcher, &test_data.diff_owned_stream.stream);

        dynmem_write(&test_data.outfile_owned_stream.mem, "\0", sizeof(char), 1);
        printf("%s", test_data.outfile_owned_stream.mem.buf);

        patch_destroy(patcher);
    }

    return 0;
}
