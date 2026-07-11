#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(arg) (void)(arg)
#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)
#define GB(x) (x * 1024 * 1024 * 1024)
#define ALIGNMENT 16

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
