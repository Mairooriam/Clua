#pragma once

#include <open62541/types.h>
#include <stdalign.h>
#include <string.h>
#include <threads.h>

#include "../core/allocator.h"
#include "core/nob.h"
#include "lexer.h"
#include "string.h"

typedef struct da_UA_NodeId {
    UA_NodeId* items;
    size_t capacity;
    size_t count;
} da_UA_NodeId;

void UA_NodeId_copy_arena(memory_arena* arena, const UA_NodeId* src, UA_NodeId* dst);

typedef struct Parser {
    arr_Tokens* tokens;
    size_t current;
    memory_arena* arena;
} Parser;
void parser_init(Parser* parser, arr_Tokens* tokens, memory_arena* arena);
da_UA_NodeId* parser_parse(memory_arena* arena, char* config);
