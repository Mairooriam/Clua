#ifndef DEBUG_BUF_H
#define DEBUG_BUF_H
#include <stddef.h>

#ifndef DBGBUF_COUNT
#define DBGBUF_COUNT 4
#endif
#ifndef DBGBUF_SIZE
#define DBGBUF_SIZE 4096
#endif

// Format into an internal rotating buffer and return pointer (ephemeral).
// Contents are overwritten on subsequent calls.
const char* dbg_printf(const char* fmt, ...);

// Return pointer to a specific internal buffer (0..DBGBUF_COUNT-1)
char* dbg_getbuf(unsigned idx);
size_t dbg_buf_size(void);

#endif  // DEBUG_BUF_H

