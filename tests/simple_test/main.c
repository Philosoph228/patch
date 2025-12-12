#include <stdio.h>

#include "../../src/patch.h"

int main() {
    static const char diff_source[] =
        "--- input.txt\r\n"
        "+++ output.txt\r\n"
        "@ -1,4 +1,7 @\r\n"
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
    apply_patch(patcher, &diff_source_stream);
    patch_destroy(patcher);

    return 0;
}
