/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "log.h"

#include <assert.h>
#include <execinfo.h>
#include <stdlib.h>

#define MAX_CALLBACKS 32
#define LOG_BACKTRACE_MAX_FRAMES 64
#define LOG_BACKTRACE_DEFAULT_DEPTH 16
typedef struct {
    log_LogFn fn;
    void* udata;
    int level;
} Callback;

static struct {
    void* udata;
    log_LockFn lock;
    int level;
    bool quiet;
    Callback callbacks[MAX_CALLBACKS];
    bool backtrace_enabled;
    int backtrace_depth;
} L = {
    .backtrace_enabled = false,
    .backtrace_depth = LOG_BACKTRACE_DEFAULT_DEPTH,
};
static void print_backtrace(FILE* out);

static const char* level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "ALLOC", "DE_ALLOC"};

#ifdef LOG_USE_COLOR
static const char* level_colors[] = {
    "\x1b[94m",              // TRACE  - bright blue
    "\x1b[36m",              // DEBUG  - cyan
    "\x1b[32m",              // INFO   - green
    "\x1b[33m",              // WARN   - yellow
    "\x1b[31m",              // ERROR  - red
    "\x1b[35m",              // FATAL  - magenta
    "\x1b[38;2;255;165;0m",  // ALLOC  - orange (24-bit RGB)
    "\x1b[38;2;255;200;60m"  // DE_ALLOC - brighter, more yellowish orange
};
#endif

static void stdout_callback(log_Event* ev) {
    char buf[16];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
#ifdef LOG_USE_COLOR
    // TODO: add colors for the function debug text
    fprintf(
        (FILE*)ev->udata,
        "%s %s%-5s\x1b[0m \x1b[90m%s:%d:%s \x1b[0m",
        buf,
        level_colors[ev->level],
        level_strings[ev->level],
        ev->file,
        ev->line,
        ev->func);
#else
    fprintf((FILE*)ev->udata, "%s %-5s %s:%d: ", buf, level_strings[ev->level], ev->file, ev->line);
#endif
    vfprintf((FILE*)ev->udata, ev->fmt, ev->ap);
    fprintf((FILE*)ev->udata, "\n");
    fflush((FILE*)ev->udata);
    if (L.backtrace_enabled && (ev->level == LOG_ERROR || ev->level == LOG_FATAL)) {
        assert(L.backtrace_depth > 0 && "Backtrace enabled, but backtrace depth was not set");
        print_backtrace((FILE*)ev->udata);
    }
}

static void file_callback(log_Event* ev) {
    char buf[64];
    buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
    fprintf((FILE*)ev->udata, "%s %-5s %s:%d: ", buf, level_strings[ev->level], ev->file, ev->line);
    vfprintf((FILE*)ev->udata, ev->fmt, ev->ap);
    fprintf((FILE*)ev->udata, "\n");
    fflush((FILE*)ev->udata);
}

static void lock(void) {
    if (L.lock) {
        L.lock(true, L.udata);
    }
}

static void unlock(void) {
    if (L.lock) {
        L.lock(false, L.udata);
    }
}

const char* log_level_string(int level) {
    return level_strings[level];
}

void log_set_lock(log_LockFn fn, void* udata) {
    L.lock = fn;
    L.udata = udata;
}

void log_set_level(int level) {
    L.level = level;
}

void log_set_quiet(bool enable) {
    L.quiet = enable;
}

int log_add_callback(log_LogFn fn, void* udata, int level) {
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!L.callbacks[i].fn) {
            L.callbacks[i] = (Callback){fn, udata, level};
            return 0;
        }
    }
    return -1;
}

int log_add_fp(FILE* fp, int level) {
    return log_add_callback(file_callback, fp, level);
}
static void print_backtrace(FILE* out) {
    int depth = L.backtrace_depth;
    if (depth < 1) depth = LOG_BACKTRACE_DEFAULT_DEPTH;
    if (depth > LOG_BACKTRACE_MAX_FRAMES) depth = LOG_BACKTRACE_MAX_FRAMES;

    void* frames[LOG_BACKTRACE_MAX_FRAMES];
    int frame_count = backtrace(frames, depth);
    if (frame_count <= 0) {
        return;
    }

    char** symbols = backtrace_symbols(frames, frame_count);
    if (!symbols) {
        return;
    }

    fprintf(out, " backtrace (%d frames):\n", frame_count);
    for (int i = 0; i < frame_count; ++i) {
        fprintf(out, " [%d] %s\n", i, symbols[i]);
    }
    fflush(out);
    free(symbols);
}
static void init_event(log_Event* ev, void* udata) {
    if (!ev->time) {
        time_t t = time(NULL);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}

void log_log(int level, const char* file, int line, const char* func, const char* fmt, ...) {
    log_Event ev = {
        .fmt = fmt,
        .file = file,
        .line = line,
        .func = func,
        .level = level,
    };

    lock();

    if (!L.quiet && level >= L.level) {
        init_event(&ev, stderr);
        va_start(ev.ap, fmt);
        stdout_callback(&ev);
        va_end(ev.ap);
    }

    for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
        Callback* cb = &L.callbacks[i];
        if (level >= cb->level) {
            init_event(&ev, cb->udata);
            va_start(ev.ap, fmt);
            cb->fn(&ev);
            va_end(ev.ap);
        }
    }

    unlock();
}

void log_set_backtrace(bool enable) {
    L.backtrace_enabled = enable;
}

void log_set_backtrace_depth(int depth) {
    if (depth < 1) depth = 1;
    if (depth > LOG_BACKTRACE_MAX_FRAMES) depth = LOG_BACKTRACE_MAX_FRAMES;
    L.backtrace_depth = depth;
}
