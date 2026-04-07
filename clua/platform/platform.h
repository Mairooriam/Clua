#ifndef CORE_PLATFORM_H
#define CORE_PLATFORM_H

#include "platform_types.h"

MirFileResult MirFileSize(MirFile* file, size_t* size);
MirFileResult MirFileOpen(const char* path, MirFile* out, MirFileAccess accesType);
MirFileResult MirFileWrite(MirFile* handle, const void* data, size_t size, size_t* bytesWritten);
void MirFileClose(MirFile* handle);

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#define CORE_PLATFORM_WINDOWS
#include "windows/wCore.h"
#else
#error "Unsupported platform"
#endif

#endif  // CORE_PLATFORM_H
