#include "allocator.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"

void HexDump(const void* data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i + 1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

size_t align(size_t n, size_t to) {
    // example n = 7, to = 8
    // 7 ---------------- 0000 0111
    // 7 + (8 - 1) = 14 - 0000 1110
    // AND NOT 7 & ~to -- 1111 1000
    // Reuslt ----------- 0000 1000 = 16
    return (n + (to - 1)) & ~(to - 1);
}

void* arena_alloc(memory_arena* arena, size_t size, size_t alignment) {
    // TODO: look into how to align correcltly. pass obj?
    size_t aligned_offset = align(arena->offset, alignment);

    // overflow-safe bounds check
    if (aligned_offset > arena->size || size > arena->size - aligned_offset) {
        log_fatal("Arena ran out of memory! no dynamic arena yet!");
        abort();
        return NULL;
    }

    void* ptr = arena->data + aligned_offset;
    arena->offset = aligned_offset + size;
    return ptr;
}

bool arena_alloc_copy(memory_arena* arena, const void* src, size_t size, size_t alignment) {
    void* dst = arena_alloc(arena, size, alignment);
    if (!dst) {
        return false;
    }
    memcpy(dst, src, size);
    return true;
}

memory_arena* arena_create(size_t size) {
    memory_arena* a = (memory_arena*)malloc(sizeof(*a));
    if (!a) return NULL;

    a->data = (char*)malloc(size);
    if (!a->data) {
        free(a);
        return NULL;
    }
    memset(a->data, 0, size);

    a->size = size;
    a->offset = 0;
    a->restorePoint = 0;
    return a;
}

void arena_init(memory_arena* a) {
    a->data = NULL;
    a->offset = 0;
    a->size = 0;
    a->restorePoint = 0;
}

void arena_reset(memory_arena* a, bool hard) {
    if (hard) {
        a->offset = 0;
    } else {
        a->offset = a->restorePoint;
    }
}
void arena_set_reset_point_current(memory_arena* a) {
    a->restorePoint = a->offset;
}

void arena_destroy(memory_arena* a) {
    if (!a) return;
    free(a->data);
    free(a);
}
char* arena_strdup(memory_arena* arena, const char* src, size_t alignment) {
    size_t len = strlen(src) + 1;
    char* dst = (char*)arena_alloc(arena, len, alignment);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    return dst;
}

// example
//  int main(int argc, char *argv[]) {
//    UNUSED(argc);
//    UNUSED(argv);
//
//    size_t mmapLen = KB(4);
//
//    memory_arena *arena = arena_create(KB(4));
//    if (!arena) {
//      printf("arena creation failed\n");
//    }
//
//    for (size_t i = 0; i < mmapLen; i++) {
//      char *c = arena_alloc(arena, sizeof(char));
//      if (!c) {
//        printf("arena exhausted at i=%zu, offset=%zu\n", i, arena->offset);
//        break;
//      }
//      *c = 'a';
//    }
//
//    HexDump(arena->data, mmapLen);
//
//    printf("Aligned 10 to 16, %zu\n", align(10, 16));
//    printf("Aligned 15 to 16, %zu\n", align(15, 16));
//    printf("Aligned 3 to 16, %zu\n", align(3, 16));
//
//    arena_destroy(arena);
//
//    printf("hello world 2\n");
//    return 0;
//  }
