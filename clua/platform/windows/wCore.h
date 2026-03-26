#pragma once
#include <stdbool.h>
#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== UTILS ====================
uint64_t combine_u32_u32(DWORD low, DWORD high);

// ==================== FILE I/O ====================
FILETIME wCore_file_get_lastWriteTime(const char *filename);
void wCore_file_FiletimeToString(char *str, size_t bufsize, FILETIME ft);

// TODO: add rest of windows utils for checking file when needed

// ==================== DLL ====================
typedef struct {
  HMODULE handle;
  FILETIME lastWriteTime;
} DLLHandle;
bool wCore_dll_load(const char *sourcePath, const char *tempPath,
                    DLLHandle *outDLL);
void wCore_dll_unload(DLLHandle *dll);
bool wCore_dll_hasChanged(const char *sourcePath, const DLLHandle *dll);
void *_wCore_dll_getFunction(DLLHandle *dll, const char *functionName);
#define wCore_dll_LOAD_DLL_FUNCTION(dll, funcPtr, funcName, funcType,          \
                                    successFlag)                               \
  do {                                                                         \
    funcPtr = (funcType)_wCore_dll_getFunction(dll, funcName);                 \
    if (!funcPtr) {                                                            \
      successFlag = false;                                                     \
    }                                                                          \
  } while (0)

#ifdef __cplusplus
}
#endif
