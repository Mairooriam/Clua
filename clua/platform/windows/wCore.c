#include "wCore.h"

#include <assert.h>
#include <fileapi.h>
#include <handleapi.h>
#include <minwindef.h>
#include <stdio.h>
#include <winnt.h>

#include "../platform.h"
#include "../platform_types.h"
uint64_t combine_u32_u32(DWORD low, DWORD high) {
    return ((uint64_t)high << 32) | low;
}

void wCore_file_FiletimeToString(char* str, size_t bufsize, FILETIME ft) {
    assert(bufsize >= 20 && "Buffer too small for date/time string");

    FILETIME localFt;
    if (!FileTimeToLocalFileTime(&ft, &localFt)) {
        snprintf(str, bufsize, "Invalid time");
        return;
    }

    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&localFt, &st)) {
        snprintf(str, bufsize, "Invalid time");
        return;
    }

    snprintf(
        str,
        bufsize,
        "%04d_%02d_%02d__%02d_%02d_%02d",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond);
}

FILETIME wCore_file_get_lastWriteTime(const char* filename) {
    uint64_t result;
    WIN32_FILE_ATTRIBUTE_DATA fInfo;
    if (GetFileAttributesExA(filename, GetFileExInfoStandard, &fInfo)) {
        return fInfo.ftLastWriteTime;
    } else {
        return (FILETIME){0};
    }
}
bool wCore_dll_load(const char* sourcePath, const char* tempPath, DLLHandle* outDLL) {
    // TODO: add some error messages incase you run into problems

    DeleteFileA(tempPath);
    outDLL->lastWriteTime = wCore_file_get_lastWriteTime(sourcePath);
    if (!CopyFile(sourcePath, tempPath, FALSE)) {
        return false;
    }

    outDLL->handle = LoadLibraryA(tempPath);
    if (!outDLL->handle) {
        return false;
    }

    return true;
}
void wCore_dll_unload(DLLHandle* dll) {
    if (dll->handle) {
        FreeLibrary(dll->handle);
    }
}
void* _wCore_dll_getFunction(DLLHandle* dll, const char* functionName) {
    if (dll->handle) {
        return (void*)GetProcAddress(dll->handle, functionName);
    }
    return NULL;
}

bool wCore_dll_hasChanged(const char* sourcePath, const DLLHandle* dll) {
    FILETIME currentTime = wCore_file_get_lastWriteTime(sourcePath);
    return CompareFileTime(&currentTime, &dll->lastWriteTime) != 0;
}

MirFileResult MirFileOpen(const char* path, MirFile* out, MirFileAccess accessType) {
    DWORD desiredAccess = 0;
    DWORD creationDisposition = 0;

    switch (accessType) {
        case MIR_FILE_ACCESS_READ:
            desiredAccess = GENERIC_READ;
            creationDisposition = OPEN_EXISTING;
            break;

        case MIR_FILE_ACCESS_WRITE:
            desiredAccess = GENERIC_WRITE;
            creationDisposition = CREATE_ALWAYS;
            break;

        case MIR_FILE_ACCESS_APPEND:
            desiredAccess = FILE_APPEND_DATA;
            creationDisposition = OPEN_ALWAYS;
            break;

        case MIR_FILE_ACCESS_READ_WRITE:
            desiredAccess = GENERIC_READ | GENERIC_WRITE;
            creationDisposition = OPEN_ALWAYS;
            break;

        default: return MIR_FILE_ERR_UNSUPPORTED_ACCESS;
    }

    HANDLE h =
        CreateFileA(path, desiredAccess, 0, NULL, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        return MIR_FILE_ERR_HANDLE_CREATION_FAILED;
    }

    out->_handle = h;
    out->type = accessType;
    return MIR_FILE_OK;
}

MirFileResult MirFileWrite(MirFile* handle, const void* data, size_t size, size_t* bytesWritten) {
    if (!handle->_handle) return MIR_FILE_ERR_INVALID_HANDLE;
    DWORD written;
    if (!WriteFile((HANDLE)handle->_handle, data, (DWORD)size, &written, NULL))
        return MIR_FILE_ERR_WRITE_FAILED;
    if (bytesWritten) *bytesWritten = written;
    return MIR_FILE_OK;
}

void MirFileClose(MirFile* handle) {
    if (handle->_handle) {
        CloseHandle((HANDLE)handle->_handle);
        handle->_handle = NULL;
    }
}
MirFileResult MirFileSize(MirFile* file, size_t* size) {
    if (!file->_handle) return MIR_FILE_ERR_INVALID_HANDLE;
    LARGE_INTEGER fileSize;

    BOOL success = GetFileSizeEx(file->_handle, &fileSize);
    // TODO: GetLastError for additional error information
    if (!success) {
        return MIR_FILE_ERR;
    }
    *size = (size_t)fileSize.QuadPart;
    return MIR_FILE_OK;
}
