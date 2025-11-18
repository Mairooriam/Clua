#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdio.h>


int safe_snprintf(char* buf, size_t* offset, size_t bufsize, const char* format, ...);
void str_remove_substring(char* str, const char* to_remove);
#endif  // UTILS_H

