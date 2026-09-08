#pragma once
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define UNUSED(arg) (void)(arg)
#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)
#define GB(x) (x * 1024 * 1024 * 1024)
#define ALIGNMENT 16

#define ARENA_DEFAULT_CAP 128

#define mir_da_arena_realloc(arena, da, ELEM_T)                                           \
    do {                                                                                  \
        if ((da)->count >= (da)->capacity) {                                              \
            size_t new_cap = (da)->capacity ? (da)->capacity * 2 : ARENA_DEFAULT_CAP;     \
            ELEM_T* new_items =                                                           \
                (ELEM_T*)arena_alloc((arena), sizeof(ELEM_T) * new_cap, alignof(ELEM_T)); \
            if (!new_items) {                                                             \
                fprintf(                                                                  \
                    stderr,                                                               \
                    "arena_alloc failed: requested %zu bytes\n",                          \
                    sizeof(ELEM_T) * new_cap);                                            \
                abort();                                                                  \
            }                                                                             \
            if ((da)->items && (da)->count > 0)                                           \
                memcpy(new_items, (da)->items, (da)->count * sizeof(ELEM_T));             \
            (da)->items = new_items;                                                      \
            (da)->capacity = new_cap;                                                     \
        }                                                                                 \
    } while (0)

#define mir_da_arena_reserve(arena, da, expected_capacity, ELEM_T)                            \
    do {                                                                                      \
        if ((expected_capacity) > (da)->capacity) {                                           \
            size_t old_count = (da)->count;                                                   \
            void* old_items = (da)->items;                                                    \
                                                                                              \
            if ((da)->capacity == 0) {                                                        \
                (da)->capacity = NOB_DA_INIT_CAP;                                             \
            }                                                                                 \
            while ((expected_capacity) > (da)->capacity) {                                    \
                (da)->capacity *= 2;                                                          \
            }                                                                                 \
                                                                                              \
            void* new_items =                                                                 \
                arena_alloc((arena), sizeof(*(da)->items) * (da)->capacity, alignof(ELEM_T)); \
            NOB_ASSERT(new_items != NULL && "Buy more RAM lol");                              \
                                                                                              \
            if (old_items && old_count > 0) {                                                 \
                memcpy(new_items, old_items, old_count * sizeof(*(da)->items));               \
            }                                                                                 \
                                                                                              \
            (da)->items = NOB_DECLTYPE_CAST((da)->items) new_items;                           \
        }                                                                                     \
    } while (0)

#define mir_da_arena_append_many(arena, da, new_items, new_items_count, ELEM_T)                   \
    do {                                                                                          \
        mir_da_arena_reserve(arena, (da), (da)->count + (new_items_count), ELEM_T);               \
        memcpy((da)->items + (da)->count, (new_items), (new_items_count) * sizeof(*(da)->items)); \
        (da)->count += (new_items_count);                                                         \
    } while (0)

// Append a NULL-terminated string to a string builder with arena
#define mir_sb_arena_append_cstr(arena, sb, cstr)        \
    do {                                                 \
        const char* s = (cstr);                          \
        size_t n = strlen(s);                            \
        mir_da_arena_append_many(arena, sb, s, n, char); \
    } while (0)

#define mir_sb_arena_append_null(arena, sb)                   \
    do {                                                      \
        mir_da_arena_append_many((arena), (sb), "", 1, char); \
    } while (0)

#include "nob.h"

#define mir_da_arena_append(arena, da, value, ELEM_T)                 \
    do {                                                              \
        mir_da_arena_reserve((arena), (da), (da)->count + 1, ELEM_T); \
        (da)->items[(da)->count++] = (value);                         \
    } while (0)

#define mir_sb_arena_append_buf(arena, da, new_items, new_items_count, ELEM_T) \
    mir_da_arena_append_many(arena, da, new_items, new_items_count, ELEM_T)

#define sb_arena_append_buf mir_sb_arena_append_buf
#define sb_arena_append_cstr mir_sb_arena_append_cstr
#define sb_arena_append_null mir_sb_arena_append_null
#define da_arena_append mir_da_arena_append

typedef struct memory_arena {
    char* data;
    size_t size;
    size_t offset;
    size_t restorePoint;
} memory_arena;

// UTILITY
void HexDump(const void* data, size_t size);
size_t align(size_t n, size_t to);

// CORE
memory_arena* arena_create(size_t size);
void arena_init(memory_arena* a);
void* arena_alloc(memory_arena* arena, size_t size, size_t alignment);
void arena_reset(memory_arena* a, bool hard);
void arena_set_reset_point_current(memory_arena* a);
void arena_destroy(memory_arena* a);

bool arena_alloc_copy(memory_arena* arena, const void* src, size_t size, size_t aligment);
char* arena_strdup(memory_arena* arena, const char* src, size_t alignment);

static int nob_sb_appendf_arena(memory_arena* arena, Nob_String_Builder* sb, const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // NOTE: the new_capacity needs to be +1 because of the null terminator.
    // However, further below we increase sb->count by n, not n + 1.
    // This is because we don't want the sb to include the null terminator. The
    // user can always sb_append_null() if they want it
    mir_da_arena_reserve(arena, sb, sb->count + n + 1, char);
    char* dest = sb->items + sb->count;
    va_start(args, fmt);
    vsnprintf(dest, n + 1, fmt, args);
    va_end(args);

    sb->count += n;

    return n;
}

#define sb_appendf_arena nob_sb_appendf_arena
