#ifndef DYNMEM_H_
#define DYNMEM_H_

#include <stdint.h>

#define DM_EOF  0x01
#define DM_FAIL 0x02
#define DM_BAD  0x04

typedef struct dynmem_ {
    char* buf;
    size_t size;
    size_t readpos;
    size_t writepos;
    uint8_t flags;
} dynmem_t;

/*
 * Makes an empty dynmem. It's buf pointer is NULL, size is 0.
 * Buffer and size will be set automatically on dynmem_write or dynmem_resize
 */
long make_dynmem(dynmem_t* dm, size_t element_count, size_t count);

/*
 * Initializes dynmem buf contents and size by copying the provided buffer in arguments
 */
long make_dynmem_as_copy(dynmem_t* dm, const char* data, size_t element_size, size_t count);

/*
 * Deallocates the memory, sets the buffer and size to 0. Basically dynmem becomes empty.
 */
long dynmem_free(dynmem_t* dm);

long dynmem_resize(dynmem_t* dm, size_t new_size);
long dynmem_write(dynmem_t* dm, const char* data, size_t element_size, size_t count);
long dynmem_read(dynmem_t* dm, char* data, size_t element_size, size_t count);
long dynmem_seekp(dynmem_t* dm, long offset, int origin);
long dynmem_seekg(dynmem_t* dm, long offset, int origin);
long dynmem_tellp(dynmem_t* dm);
long dynmem_tellg(dynmem_t* dm);

#endif  /* DYNMEM_H_ */
