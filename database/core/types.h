#pragma once
#include <stdint.h>

#define f32 float
#define f64 double
#define u64 size_t
#define u32 uint32_t

#define i64 int64_t
#define i32 int32_t
// #define i16 int16_t
// #define i8 int8_t

#include "../core/allocator.h"

typedef struct arr_f32 {
    f32* items;
    u64 count;
    u64 capacity;
} arr_f32;
arr_f32* arr_f32_create_in_arena(memory_arena* arena, size_t count);

typedef struct arr_i64 {
    i64* items;
    u64 count;
    u64 capacity;
} arr_i64;
arr_i64* arr_i64_create_in_arena(memory_arena* arena, size_t count);
