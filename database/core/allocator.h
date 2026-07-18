#pragma once
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(arg) (void)(arg)
#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)
#define GB(x) (x * 1024 * 1024 * 1024)
#define ALIGNMENT 16

#define ARENA_DEFAULT_CAP 128

#define DA_ARENA_REALLOC(arena, da, ELEM_T)                                               \
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

/* Use inside a function to push `value` into `da` using `arena`. Returns false on alloc failure. */
#define ARENA_PUSH(arena, da, ELEM_T, value)     \
    do {                                         \
        DA_ARENA_REALLOC((arena), (da), ELEM_T); \
        (da)->items[(da)->count++] = (value);    \
    } while (0)

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
