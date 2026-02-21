#include "utils.h"

#include <stdarg.h> 

int safe_snprintf(char* buf, size_t* offset, size_t bufsize, const char* format, ...) {
    va_list args;
    va_start(args, format);
    size_t available = bufsize - *offset;
    int written = vsnprintf(buf + *offset, available, format, args);
    va_end(args);

    if (written < 0 || (size_t)written >= available) {
        return -1;
    }
    *offset += (size_t)written;
    return 0;
}