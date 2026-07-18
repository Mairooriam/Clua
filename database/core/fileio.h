#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "allocator.h"
#include "log.h"

char* fs_read_file(const char* filepath, memory_arena* arena);
int64_t now_ms(void);
bool fs_file_has_changed(const char* filepath, time_t lastTouch);
time_t fs_file_get_last_touch(const char* filepath);

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
