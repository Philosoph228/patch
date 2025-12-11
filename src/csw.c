#include <stdio.h>

#include "csw.h"

long make_fdsw(void* self, FILE* fp) {
    if (fp == NULL)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    if (sw->_impl != NULL) /* stream descriptor already opened and used */
        return -1;

    FILE** _fp = (FILE**)&sw->_impl;
    *_fp = fp;

    sw->read = &fdsw_read;
    sw->write = &fdsw_write;
    sw->tellg = &fdsw_tellg;
    sw->tellp = &fdsw_tellp;
    sw->seekg = &fdsw_seekg;
    sw->seekp = &fdsw_seekp;
    sw->close = &fdsw_close;

    return 0;
}

long fdsw_read(void* self, char* data, size_t element_size, size_t count) {
    if (self == NULL)
        return -1;

    if (data == NULL || element_size == 0 || count == 0)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    FILE* fp = (FILE*)sw->_impl;
    if (fp == NULL)
        return -1;

    return fread(data, element_size, count, fp);
}

long fdsw_write(void* self, const char* data, size_t element_size, size_t count) {
    if (self == NULL)
        return -1;

    if (data == NULL || element_size == 0 || count == 0)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    FILE* fp = (FILE*)sw->_impl;
    if (fp == NULL)
        return -1;

    return fwrite(data, element_size, count, fp);
}

long fdsw_seekg(void* self, size_t pos, int whence) {
    if (self == NULL)
        return -1;
    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    FILE* fp = (FILE*)sw->_impl;
    if (fp == NULL)
        return -1;

    long new_pos = sw->read_pos;
    switch (whence) {
    case SEEK_SET:
        new_pos = (long)pos;
        break;
    case SEEK_CUR:
        new_pos += (long)pos;
        break;
    case SEEK_END:
        if (fseek(fp, 0, SEEK_END) != 0)
            return -1;
        new_pos = ftell(fp) + pos;
        break;
    default:
        return -1;
    }
    if (new_pos < 0)
        return -1;
    sw->read_pos = new_pos;
    return 0;
}

long fdsw_seekp(void* self, size_t pos, int whence) {
    if (self == NULL)
        return -1;
    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    FILE* fp = (FILE*)sw->_impl;
    if (fp == NULL)
        return -1;

    long new_pos = sw->write_pos;
    switch (whence) {
    case SEEK_SET:
        new_pos = (long)pos;
        break;
    case SEEK_CUR:
        new_pos += (long)pos;
        break;
    case SEEK_END:
        if (fseek(fp, 0, SEEK_END) != 0)
            return -1;
        new_pos = ftell(fp) + pos;
        break;
    default:
        return -1;
    }
    if (new_pos < 0)
        return -1;
    sw->write_pos = new_pos;
    return 0;
}

long fdsw_tellg(void* self) {
    if (self == NULL)
        return -1;
    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    return sw->read_pos;
}

long fdsw_tellp(void* self) {
    if (self == NULL)
        return -1;
    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    return sw->write_pos;
}

long fdsw_close(void* self) {
    if (self == NULL)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    FILE* fp = (FILE*)sw->_impl;
    if (fp == NULL)
        return -1;

    int status = fclose(fp);
    if (status == 0) {
        /* 0 means success */
        fp = NULL;
    }

    return status;
}

long make_memsw(void* self, dynmem_t* dm) {
    if (dm == NULL) /* Invalid dynmem_t* */
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    if (sw->_impl != NULL) /* stream descriptor already opened and used */
        return -1;

    dynmem_t** _dm = (dynmem_t**)&sw->_impl;
    *_dm = dm;

    sw->read = &memsw_read;
    sw->write = &memsw_write;
    sw->tellg = &memsw_tellg;
    sw->tellp = &memsw_tellp;
    sw->seekg = &memsw_seekg;
    sw->seekp = &memsw_seekp;
    sw->close = &memsw_close;

    return 0;
}

long memsw_read(void* self, char* data, size_t element_size, size_t count) {
    if (self == NULL) /* Invalid self pointer */
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    dynmem_t* dm = (dynmem_t*)sw->_impl;
    if (dm == NULL) /* Invalid dynmem pointer */
        return -1;

    if (data == NULL) /* Invalid data pointer to read to */
        return -1;

    if (element_size == 0 || count == 0) /* Nothing to read */
        return 0;

    return dynmem_read(dm, data, element_size, count);
}

long memsw_write(void* self, const char* data, size_t element_size, size_t count) {
    if (self == NULL)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    dynmem_t* dm = (dynmem_t*)sw->_impl;
    if (dm == NULL) /* Invalid dynmem pointer */
        return -1;

    if (data == NULL) /* Invalid data pointer to write from */
        return -1;

    if (element_size == 0 || count == 0) /* Nothing to read */
        return 0;

    return dynmem_write(dm, data, element_size, count);
}

long memsw_seekp(void* self, size_t pos, int whence) {
    if (self == NULL)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    dynmem_t* dm = (dynmem_t*)sw->_impl;
    if (dm == NULL) /* Invalid dynmem pointer */
        return -1;

    /* dynmem_seekg expects a long offset; cast pos -> long (same pattern as fdsw) */
    long offset = (long)pos;
    return dynmem_seekp(dm, offset, whence);
}

long memsw_seekg(void* self, size_t pos, int whence) {
    if (self == NULL)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    dynmem_t* dm = (dynmem_t*)sw->_impl;
    if (dm == NULL) /* Invalid dynmem pointer */
        return -1;

    long offset = (long)pos;
    return dynmem_seekg(dm, offset, whence);
}

long memsw_tellp(void* self) {
    if (self == NULL)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    dynmem_t* dm = (dynmem_t*)sw->_impl;
    if (dm == NULL) /* Invalid dynmem pointer */
        return -1;

    return dynmem_tellg(dm);
}

long memsw_tellg(void* self) {
    if (self == NULL)
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    dynmem_t* dm = (dynmem_t*)sw->_impl;
    if (dm == NULL) /* Invalid dynmem pointer */
        return -1;

    return dynmem_tellp(dm);
}

long memsw_close(void* self) {
    if (self == NULL) /* Invalid self pointer */
        return -1;

    stream_wrapper_t* sw = (stream_wrapper_t*)self;
    dynmem_t* dm = (dynmem_t*)sw->_impl;
    if (dm == NULL) /* Invalid dynmem pointer */
        return -1;

    return dynmem_free(dm);
}
