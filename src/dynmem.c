#include <stdio.h>  /* for std seek whence macros */
#include <stdlib.h> /* for malloc() */
#include <stdint.h> /* for uint8_t */
#include <string.h> /* for memcpy */

#include "dynmem.h"

/* Helper: safe add/sub for (signed) long and size_t, result in signed long long */
static int compute_new_pos_from_long(size_t base, long offset, int origin,
                                     size_t* out_newpos) {
    /* compute base_pos depending on origin (base already passed in appropriately) */
    /* We're going to calculate newpos = (signed)base + offset for SEEK_CUR,
       or offset for SEEK_SET, or (signed)base + offset for SEEK_END.
       But to avoid signed/unsigned pitfalls, use signed 128-ish arithmetic via long long. */

    long long base_ll = (long long)base;
    long long newpos_ll = 0;

    if (origin == SEEK_SET) {
        newpos_ll = (long long)offset;
    } else if (origin == SEEK_CUR || origin == SEEK_END) {
        newpos_ll = base_ll + (long long)offset;
    } else {
        return -1; /* invalid origin */
    }

    if (newpos_ll < 0)
        return -1; /* negative position not allowed */
    if ((unsigned long long)newpos_ll > (unsigned long long)SIZE_MAX)
        return -1; /* overflow */
    *out_newpos = (size_t)newpos_ll;
    return 0;
}

/*
 *  PUBLIC API
 */

long make_dynmem(dynmem_t* dm, size_t element_size, size_t count) {
    if (dm == NULL)
        return -1;

    dm->buf = NULL;
    dm->size = 0;
    dm->readpos = 0;
    dm->writepos = 0;

    /* Optional: pre-allocate buffer if parameters specify a size */
    if (element_size != 0 && count != 0) {
        size_t total = element_size * count;

        if (dynmem_resize(dm, total) != 0)
            return -1;
    }

    return 0;
}

long make_dynmem_as_copy(dynmem_t* dm, const char* data, size_t element_size, size_t count) {
    if (dm == NULL || data == NULL)
        return -1;

    size_t total = element_size * count;

    dm->buf = NULL;
    dm->size = 0;
    dm->readpos = 0;
    dm->writepos = 0;

    /* Allocate exact size */
    if (dynmem_resize(dm, total) != 0)
        return -1;

    /* Copy data into buffer */
    memcpy(dm->buf, data, total);
    dm->writepos = total;

    return 0;
}

long dynmem_free(dynmem_t* dm) {
    if (dm == NULL)
        return -1;
    free(dm->buf);
    dm->buf = NULL;
    dm->size = 0;
    dm->readpos = 0;
    dm->writepos = 0;

    return 0;
}

long dynmem_resize(dynmem_t* dm, size_t new_size) {
    if (dm == NULL) /* Invalid self pointer */
        return -1;
    void* new_ptr = NULL;
    if (dm->buf == NULL) {
        new_ptr = malloc(new_size);
        if (new_ptr == NULL) /* failed to allocate */
            return -1;
    } else {
        new_ptr = realloc(dm->buf, new_size);
        if (new_ptr == NULL) /* failed to allocate */
            return -1;
    }
    dm->buf = (char*)new_ptr;
    dm->size = new_size;
    return 0;
};

long dynmem_write(dynmem_t* dm, const char* data, size_t element_size, size_t count) {
    if (dm == NULL) /* Invalid self pointer */
        return -1;
    if (data == NULL) /* Invalid data pointer to copy data from */
        return -1;
    if (element_size == 0 || count == 0) /* Nothing to write, just return */
        return 0;
    size_t total = element_size * count; /* Total bytes to write */
    size_t endpos = dm->writepos + total; /* Location where write will end */
    if (endpos > dm->size) {
        /* Grow buffer only if needed */
        if (dynmem_resize(dm, endpos) != 0)
            return -1;
    }
    memcpy(dm->buf + dm->writepos, data, total); /* Overwrite bytes */
    dm->writepos = endpos; /* advance write position */
    return (long)count;
}

long dynmem_read(dynmem_t* dm, char* data, size_t element_size, size_t count) {
    if (dm == NULL) /* Invalid self pointer */
        return -1;
    if (data == NULL) /* Invalid destination pointer */
        return -1;
    if (element_size == 0 || count == 0) /* Nothing to read, just return */
        return 0;
    if (dm->readpos >= dm->writepos) {
        /* Bytes available from current read position to end of written data */
        dm->flags |= DM_EOF;
        return 0; /* Nothing to read (EOF-like) */
    }
    size_t available_bytes = dm->writepos - dm->readpos;
    size_t requested_bytes = element_size * count;
    /* Number of bytes to read is the minimum */
    size_t toread_bytes = requested_bytes;
    if (toread_bytes > available_bytes)
        toread_bytes = available_bytes;
    /* Number of full elements we can return */
    size_t elements_read = toread_bytes / element_size;
    if (elements_read == 0)
        return 0;
    toread_bytes = elements_read * element_size;
    memcpy(data, dm->buf + dm->readpos, toread_bytes);
    dm->readpos += toread_bytes;
    return (long)elements_read;
}

/* Seek write position (seekp). 0 on success, -1 on error. */
long dynmem_seekp(dynmem_t* dm, long offset, int origin) {
    if (dm == NULL)
        return -1;
    size_t base;
    switch (origin) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
    case SEEK_END:      /* end == current logical end (writepos) */
        base = dm->writepos;
        break;
    default:
        return -1;
    }
    size_t newpos;
    if (compute_new_pos_from_long(base, offset, origin, &newpos) != 0)
        return -1;
    /* Do not resize here — like fseek, allow seeking past EOF. */
    dm->writepos = newpos;
    /* seekp clears the EOF flag */
    dm->flags &= ~DM_EOF;
    return 0;
}

/* Seek read position (seekg). 0 on success, -1 on error. */
long dynmem_seekg(dynmem_t* dm, long offset, int origin) {
    if (dm == NULL)
        return -1;
    size_t base;
    switch (origin) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = dm->readpos;
        break;
    case SEEK_END:  /* end is logical end (writepos) */
        base = dm->writepos;
        break;
    default:
        return -1;
    }
    size_t newpos;
    if (compute_new_pos_from_long(base, offset, origin, &newpos) != 0)
        return -1;
    /* Seeking beyond writepos is allowed; reading from beyond will return 0. */
    dm->readpos = newpos;
    return 0;
}

/* tellp: return current write position or -1 on error */
long dynmem_tellp(dynmem_t* dm) {
    if (dm == NULL)
        return -1;
    if (dm->writepos > (size_t)LONG_MAX)
        return -1; /* can't represent */
    return (long)dm->writepos;
}

/* tellg: return current read position or -1 on error */
long dynmem_tellg(dynmem_t* dm) {
    if (dm == NULL)
        return -1;
    if (dm->readpos > (size_t)LONG_MAX)
        return -1; /* can't represent */
    return (long)dm->readpos;
}
