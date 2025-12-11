#ifndef CSW_H_
#define CSW_H_

#include "dynmem.h"

typedef struct stream_wrapper_ {
    void* _impl;
    long (*read)(void* self, char* data, size_t element_size, size_t count);
    long (*write)(void* self, char* data, size_t element_size, size_t count);
    long (*seekg)(void* self, size_t pos, int whence);
    long (*seekp)(void* self, size_t pos, int whence);
    long (*tellg)(void* self);
    long (*tellp)(void* self);
    long (*close)(void* self);

    long read_pos;
    long write_pos;
} stream_wrapper_t;

long make_fdsw(void* sw, FILE* fp);
long fdsw_read(void* self, char* data, size_t element_size, size_t count);
long fdsw_write(void* self, const char* data, size_t element_size, size_t count);
long fdsw_seekg(void* self, size_t pos, int whence);
long fdsw_seekp(void* self, size_t pos, int whence);
long fdsw_tellg(void* self);
long fdsw_tellp(void* self);
long fdsw_close(void* self);

long make_memsw(void* sw, dynmem_t* dm);
long memsw_read(void* self, char* data, size_t element_size, size_t count);
long memsw_write(void* self, const char* data, size_t element_size, size_t count);
long memsw_seekg(void* self, size_t pos, int whence);
long memsw_seekp(void* self, size_t pos, int whence);
long memsw_tellg(void* self);
long memsw_tellp(void* self);
long memsw_close(void* self);

#endif  /* CSW_H_ */
