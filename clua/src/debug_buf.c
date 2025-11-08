#include "debug_buf.h"
#include <stdio.h>
#include <stdarg.h>

static char bufs[DBGBUF_COUNT][DBGBUF_SIZE];
static unsigned bufs_idx = 0;

const char* dbg_printf(const char* fmt, ...) {
    unsigned i = (++bufs_idx) % DBGBUF_COUNT;
    char* b = bufs[i];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, DBGBUF_SIZE, fmt, ap);
    va_end(ap);
    return b;
}

char* dbg_getbuf(unsigned idx) {
    return bufs[idx % DBGBUF_COUNT];
}

size_t dbg_buf_size(void) {
    return DBGBUF_SIZE;
}