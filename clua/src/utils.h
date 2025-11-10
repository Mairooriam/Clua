#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdio.h>


int safe_snprintf(char* buf, size_t* offset, size_t bufsize, const char* format, ...);

#endif  // UTILS_H

