#pragma once

#include <open62541/types.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>
#include <threads.h>

// #include "../core/allocator.h"
#include <core/allocator.h>
#include <core/nob.h>

#include "core/lexer.h"
#include "string.h"

typedef enum ValueType {
    VT_SENTINEL,
    VT_INT,
    VT_FLOAT,
    VT_STRING,
    VT_UNSUPPORTED,
} ValueType;
const char* parser_value_type_to_string(ValueType type);

typedef struct ParsedNode {
    char* name;
    UA_NodeId nodeId;
    int64_t polling;
    bool historizing;
    ValueType type;
} ParsedNode;

typedef struct da_ParsedNode {
    ParsedNode* items;
    size_t capacity;
    size_t count;
} da_ParsedNodes;

void UA_NodeId_copy_arena(memory_arena* arena, const UA_NodeId* src, UA_NodeId* dst);

typedef struct Parser {
    arr_Tokens* tokens;
    size_t current;
    memory_arena* arena;
} Parser;
void parser_init(Parser* parser, arr_Tokens* tokens, memory_arena* arena);
da_ParsedNodes* parser_parse(memory_arena* arena, char* config);
