#include "utils.h"

#include <stdarg.h> 
#include <string.h>
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

void str_remove_substring(char* str, const char* to_remove) {
    if (!str || !to_remove || strlen(to_remove) == 0) return;

    size_t len_remove = strlen(to_remove);
    char* pos = str;
    while ((pos = strstr(pos, to_remove)) != NULL) {
        memmove(pos, pos + len_remove, strlen(pos + len_remove) + 1);
    }
}