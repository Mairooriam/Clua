#pragma once
#include <stdio.h>
#include <stdbool.h>
char* mir_utils_getCurrentDirectory(void);

int mir_utils_countLines(FILE* file);

bool mir_utils_fileExistsInDirectory(const char* path, const char* filename);