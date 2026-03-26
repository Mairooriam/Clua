#include "wCore.h"
#include <assert.h>
#include <stdio.h>
uint64_t combine_u32_u32(DWORD low, DWORD high) {
  return ((uint64_t)high << 32) | low;
}

void wCore_file_FiletimeToString(char *str, size_t bufsize, FILETIME ft) {
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

  snprintf(str, bufsize, "%04d_%02d_%02d__%02d_%02d_%02d", st.wYear, st.wMonth,
           st.wDay, st.wHour, st.wMinute, st.wSecond);
}

FILETIME wCore_file_get_lastWriteTime(const char *filename) {
  uint64_t result;
  WIN32_FILE_ATTRIBUTE_DATA fInfo;
  if (GetFileAttributesExA(filename, GetFileExInfoStandard, &fInfo)) {
    return fInfo.ftLastWriteTime;
  } else {
    return (FILETIME){0};
  }
}
bool wCore_dll_load(const char *sourcePath, const char *tempPath,
                    DLLHandle *outDLL) {
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
void wCore_dll_unload(DLLHandle *dll) {
  if (dll->handle) {
    FreeLibrary(dll->handle);
  }
}
void *_wCore_dll_getFunction(DLLHandle *dll, const char *functionName) {
  if (dll->handle) {
    return (void *)GetProcAddress(dll->handle, functionName);
  }
  return NULL;
}

bool wCore_dll_hasChanged(const char *sourcePath, const DLLHandle *dll) {
    FILETIME currentTime = wCore_file_get_lastWriteTime(sourcePath);
    return CompareFileTime(&currentTime, &dll->lastWriteTime) != 0;
}
