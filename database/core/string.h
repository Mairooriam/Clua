#pragma once
#include "../core/allocator.h"
#include "stdbool.h"
#include <string.h>
#define SV_FTM "%.*s"
#define SV_ARG(sv) ((int)sv.count), sv.data
#define SV_LIT(s) ((Sv){.data = (s), .count = sizeof(s) - 1})

#define C_RESET "\x1b[0m"

#define C_BLACK "\x1b[30m"
#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_MAGENTA "\x1b[35m"
#define C_CYAN "\x1b[36m"
#define C_WHITE "\x1b[37m"

#define C_BOLD "\x1b[1m"
#define C_DIM "\x1b[2m"
#define C_UNDER "\x1b[4m"

// Optional bright variants
#define C_BRED "\x1b[91m"
#define C_BGREEN "\x1b[92m"
#define C_BYELLOW "\x1b[93m"
#define C_BBLUE "\x1b[94m"
#define C_BMAGENTA "\x1b[95m"
#define C_BCYAN "\x1b[96m"

typedef struct Sv {
  const char *data;
  size_t count;
} Sv;

Sv sv_create_from_cstr(const char *str);
bool sv_equal(Sv a, Sv b);
Sv sv2_chop_by_delim(Sv *sv, char delim);
Sv sv2_from_parts(const char *data, size_t count);
char *sv_to_cstr_arena(memory_arena *arena, Sv sv);
