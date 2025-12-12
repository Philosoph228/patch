#include <stdio.h>
#include <string.h>

#include "patch.h"

int main(int argc, char** argv) {
    /* Simple argument parser (no fancy lib). */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [--verbose] <patchfile>\n", argv[0]);
        return 1;
    }

    unsigned int options = 0;
    const char* patchfile = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--verbose") == 0)
            options |= PATCH_OPTION_VERBOSE;
        else if (*argv[i] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        } else {
            patchfile = argv[i];
        }
    }
    if (!patchfile) {
        fprintf(stderr, "Patch file not specified\n");
        return 1;
    }

    FILE* fp = fopen(patchfile, "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", patchfile);
        return 1;
    }

    stream_wrapper_t file_sw = {0};
    make_fdsw(&file_sw, fp);

    void* patcher = patch_init();
    if (patcher == NULL) {
        fprintf(stderr, "Cannot init patcher instance");
        return -1;
    }

    patch_set_options(patcher, options);
    int stat = apply_patch(patcher, &file_sw);
    patch_destroy(patcher);

    return stat;
}
