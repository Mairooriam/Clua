#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdio.h>

char* mir_utils_getCurrentDirectory(void);

int mir_utils_countLines(FILE* file);

bool mir_utils_fileExistsInDirectory(const char* path, const char* filename);

#endif  // UTILS_H

