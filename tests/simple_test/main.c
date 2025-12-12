#include <stdio.h>
#include <string.h>

#include "../../src/patch.h"

typedef struct simple_test_data {
    dynmem_t input_txt;
    dynmem_t output_txt;
} simple_test_data_t;

static const char input_txt_source[] =
    "int main() {\r\n"
    "  return 0;\r\n"
    "}\r\n"
    "\r\n";

static const char output_txt_source[] =
    "#include <stdio.h>\r\n"
    "\r\n"
    "int main() {\r\n"
    "  printf(\"Hello, my world!\");\r\n"
    "  return 0;\r\n"
    "}\r\n"
    "\r\n";

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
            if (strcmp(path, "input.txt") == 0) {
                if (dat->input_txt.buf != NULL)
                    dynmem_free(&dat->input_txt);
                make_dynmem_as_copy(&dat->input_txt, input_txt_source, sizeof(input_txt_source), 1);
                make_memsw(sw, &dat->input_txt);
                return 0;
            } else if (strcmp(path, "output.txt.tmp") == 0) {
                if (dat->output_txt.buf != NULL)
                    dynmem_free(&dat->output_txt);
                make_dynmem(&dat->output_txt, 0, 0);
                make_memsw(sw, &dat->output_txt);
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

int main() {
    simple_test_data_t test_data = { 0 };

    static const char diff_source[] =
        "--- input.txt\r\n"
        "+++ output.txt\r\n"
        "@@ -1,4 +1,7 @@\r\n"
        "+#include <stdio.h>\r\n"
        "+\r\n"
        " int main() {\r\n"
        "+  printf(\"Hello, my world!\");\r\n"
        " return 0;\r\n"
        " }\r\n"
        " \r\n";

    dynmem_t diff_source_mem = { 0 };
    dynmem_write(&diff_source_mem, diff_source, sizeof(diff_source), 1);

    stream_wrapper_t diff_source_stream = { 0 };
    make_memsw(&diff_source_stream, &diff_source_mem);

    void* patcher = patch_init();
    patch_set_options(patcher, PATCH_OPTION_VERBOSE);
    patch_set_path_cbk(patcher, (patch_event_cbk_t*)&test_cbk, (void*)&test_data);
    apply_patch(patcher, &diff_source_stream);

    printf("%s", test_data.output_txt.buf);

    patch_destroy(patcher);

    

    return 0;
}
