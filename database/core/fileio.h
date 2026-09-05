#pragma once

#include <assert.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "allocator.h"
#include "log.h"
#include "nob.h"

char* fs_read_file(const char* filepath, memory_arena* arena);
int64_t now_ms(void);
bool fs_file_has_changed(const char* filepath, time_t lastTouch);
time_t fs_file_get_last_touch(const char* filepath);
int fs_get_executable_dir(char* buf, size_t size);
int fs_sb_get_executable_dir(memory_arena* arena, String_Builder* sb);
char* fs_read_file(const char* filepath, memory_arena* arena) {
    char* buf = NULL;
    FILE* f = fopen(filepath, "r");
    if (!f) {
        log_warn("[FILE I/O] - No file %s found!", filepath);
        return buf;
    }
    struct stat sb;
    if (stat(filepath, &sb) == -1) {
        log_warn("[FILE I/O] - stat failed");
        return buf;
    }

    if (sb.st_size == 0) {
        log_warn("[FILE I/O] - File was found but size is 0");
    } else {
        buf = (char*)arena_alloc(arena, (size_t)(sb.st_size + 1), alignof(char));
        if (buf == NULL) {
            log_warn("[FILE I/O] - arena_alloc failed");
            return buf;
        }

        size_t got = fread(buf, 1, (size_t)sb.st_size, f);
        if (got != (size_t)sb.st_size) {
            if (ferror(f)) {
                log_warn("[FILE I/O] - fread failed");
                fclose(f);
                return buf;
            }
        }
        buf[got] = '\0';
        fclose(f);
    }
    return buf;
}
int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
bool fs_file_has_changed(const char* filepath, time_t lastTouch) {
    struct stat st;
    stat(filepath, &st);

    if (lastTouch != st.st_mtime) {
        lastTouch = st.st_mtime;
        return true;
    } else {
        return false;
    }
}
time_t fs_file_get_last_touch(const char* filepath) {
    struct stat st;
    stat(filepath, &st);
    return st.st_mtime;
}

int fs_get_executable_dir(char* buf, size_t bufsize) {
    char _buf[PATH_MAX];
    ssize_t size = readlink("/proc/self/exe", _buf, sizeof(_buf) - 1);
    if (size < 0) {
        return -1;
    }
    _buf[size] = '\0';

    char* slash = strrchr(_buf, '/');
    if (slash) {
        *slash = '\0';
    }

    size_t dir_len = strlen(_buf);
    if (buf == NULL || bufsize == 0) {
        return (int)(dir_len + 1);
    }

    if (bufsize < dir_len + 1) {
        return -1;
    }

    memcpy(buf, _buf, dir_len + 1);
    return (int)(dir_len + 1);
}
int fs_sb_get_executable_dir(memory_arena* arena, String_Builder* sb) {
    int required = fs_get_executable_dir(NULL, 0);
    if (required < 0) {
        return -1;
    }
    mir_da_arena_reserve(arena, sb, (size_t)required, char);
    int count = fs_get_executable_dir(sb->items, sb->capacity);
    int countWithOutNull = count - 1;
    if (count > 0) {
        // -1 for removing the null termination since SB is range based
        sb->count = (size_t)count - 1;
    }
    return countWithOutNull;
}
