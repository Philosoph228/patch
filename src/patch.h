#ifndef PATCH_H_INCLUDED_
#define PATCH_H_INCLUDED_

#include "csw.h"

#define PATCH_OPTION_INPLACE    0x1
#define PATCH_OPTION_APPLYDATES 0x2
#define PATCH_OPTION_VERBOSE    0x4

#define PATCH_EVT_STREAM_ACQUIRE 0x1
#define PATCH_EVT_STREAM_RELEASE 0x2

typedef struct patch_evt {
    unsigned int type;
    void* userdata;
    union {
        struct {
            char* path;
            stream_wrapper_t* stream;
        } stream_event;
    } data;
} patch_evt_t;

typedef int (path_cbk_t)(char* str, stream_wrapper_t* stream, void* userdata);

/* Init patcher instance
 *
 * returns pointer to the instance
 */
void* patch_init();

/* Free the patcher instance
 *
 * returns 0 on success, non-0 on error
 */
int patch_destroy(void* self);

/* Set options for the patcher instance
 *
 * returns 0 on success, non-0 on error
 */
int patch_set_options(void* self, unsigned int opts);

/* Set callback for opening input and output files for patch
 *
 * returns 0 on success, non-0 on error
 */
int patch_set_path_cbk(void* self, path_cbk_t* new_cbk, void* userdata);

/*
 * Load the diff from stream and do the work
 *
 * returns 0 on success, non-0 on error
 */
int apply_patch(void* self, stream_wrapper_t* sw);

#endif  /* PATCH_H_INLCUDED_ */
