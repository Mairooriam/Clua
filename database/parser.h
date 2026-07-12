#pragma once

#include <open62541/types.h>
#include <stdalign.h>
#include <string.h>
#include <threads.h>

#include "../core/allocator.h"
#include "lexer.h"
#include "nob.h"
#include "string.h"

typedef struct arr_UA_NodeId {
    UA_NodeId* items;
    size_t capacity;
    size_t count;
} da_UA_NodeId;

typedef struct Parser {
    arr_Tokens* tokens;
    size_t current;
    memory_arena* arena;
} Parser;
void parser_init(Parser* parser, arr_Tokens* tokens, memory_arena* arena);
da_UA_NodeId* parser_parse(Parser* p);
