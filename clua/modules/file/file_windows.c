#include "file_module.h"
#include <stdlib.h>
#include <windows.h>
#include <stdbool.h>
char* mir_utils_getCurrentDirectory(void) {
    DWORD size = GetCurrentDirectory(0, NULL);
    if (size == 0) return NULL;

    char* buffer = (char*)malloc(size);
    if (!buffer) return NULL;

    DWORD res = GetCurrentDirectory(size, buffer);
    if (res == 0 || res >= size) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

// https://stackoverflow.com/questions/12733105/c-function-that-counts-lines-in-file
#define BUF_SIZE 65536
int mir_utils_countLines(
    FILE* file)  // TODO: look into why it doenst count text with "\n". which is good but to understand
{
    char buf[BUF_SIZE];
    int counter = 0;
    for (;;) {
        size_t res = fread(buf, 1, BUF_SIZE, file);
        if (ferror(file)) return -1;
        if (res == 0) break;

        for (size_t i = 0; i < res; i++)
            if (buf[i] == '\n') counter++;

        if (feof(file)) break;
    }

    return counter;
}
bool mir_utils_fileExistsInDirectory(const char* path, const char* filename) {
    size_t path_len = strlen(path);
    size_t filename_len = strlen(filename);
    size_t full_len = path_len + 1 + filename_len + 1; 
    char* full_path = (char*)malloc(full_len);
    if (!full_path) return false;

    strcpy(full_path, path);
    if (path_len > 0 && full_path[path_len - 1] != '\\') {
        strcat(full_path, "\\");
    }
    strcat(full_path, filename);

    DWORD attributes = GetFileAttributes(full_path);
    free(full_path);

    // Return true if attributes are valid (file exists) and it's not a directory
    return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}