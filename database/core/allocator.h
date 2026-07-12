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

#define ARENA_DEFINE_PUSH_FN(FN_NAME, ARR_T, ELEM_T, DEFAULT_CAP)                       \
    static bool FN_NAME(memory_arena* arena, ARR_T* out, ELEM_T value) {                \
        if (out->count >= out->capacity) {                                              \
            size_t new_cap = out->capacity ? out->capacity * 2 : (DEFAULT_CAP);         \
            ELEM_T* new_items =                                                         \
                (ELEM_T*)arena_alloc(arena, sizeof(ELEM_T) * new_cap, alignof(ELEM_T)); \
            if (!new_items) return false;                                               \
            if (out->items && out->count > 0) {                                         \
                memcpy(new_items, out->items, out->count * sizeof(ELEM_T));             \
            }                                                                           \
            out->items = new_items;                                                     \
            out->capacity = new_cap;                                                    \
        }                                                                               \
        out->items[out->count++] = value;                                               \
        return true;                                                                    \
    }
// Example:
// ARENA_DEFINE_PUSH_FN(lx_tokens_push, arr_Tokens, Token, 128)

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
