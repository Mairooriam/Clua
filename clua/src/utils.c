#include "utils.h"

#include <stdlib.h>
#include <windows.h>
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
bool mir_utils_fileExistsInDirectory(const char* path, const char* filename) {  // TODO: make into nice function
    bool configFound = false;
    const char* filter = "\\*";  // TODO: make dynamic not hard coded? why? idk?

    // building temp string to compare to.
    size_t len = strlen(path) + strlen(filter) + 1;
    char* searchPath = (char*)malloc(len);
    if (!searchPath) return false;
    strcpy(searchPath, path);
    strcat(searchPath, filter);

    // windows specific stuff
    WIN32_FIND_DATA ffd;
    LARGE_INTEGER filesize;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    DWORD dwError = 0;

    hFind = FindFirstFile(searchPath, &ffd);
    free(searchPath);

    if (INVALID_HANDLE_VALUE == hFind) {
        dwError = GetLastError();
        printf("Couldn't find config, error code %lu in: %s\n", dwError, path);
        printf(
            "See Windows System Error Codes: "
            "https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes\n");  // TODO: add proper text
        printf("Exiting bye!");                                                             // for theerrors not
                                                                                            // just number
        return -1;
    }
    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            printf("  %s   <DIR>\n", ffd.cFileName);
        } else {
            filesize.LowPart = ffd.nFileSizeLow;
            filesize.HighPart = ffd.nFileSizeHigh;
            printf("  %s   %lld bytes\n", ffd.cFileName, filesize.QuadPart);
            if (strcmp(ffd.cFileName, filename) == 0) {
                configFound = true;
            }
        }

    } while (FindNextFile(hFind, &ffd) != 0);
    FindClose(hFind);
    return configFound;
}
