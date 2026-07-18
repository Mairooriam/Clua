#pragma once

#include <open62541/types.h>
#include <stdalign.h>
#include <string.h>
#include <threads.h>

#include "../core/allocator.h"
#include "lexer.h"
#include "nob.h"
#include "string.h"

typedef struct da_UA_NodeId {
    UA_NodeId* items;
    size_t capacity;
    size_t count;
} da_UA_NodeId;
// typedef struct {
//     UA_UInt16 namespaceIndex;
//     enum UA_NodeIdType identifierType;
//     union {
//         UA_UInt32     numeric;
//         UA_String     string;
//         UA_Guid       guid;
//         UA_ByteString byteString;
//     } identifier;
// } UA_NodeId;
// typedef uint32_t UA_UInt32;
// #define UA_UINT32_MIN 0
// #define UA_UINT32_MAX 4294967295UL
// typedef struct {
//     size_t length; /* The length of the string */
//     UA_Byte *data; /* The content (not null-terminated) */
// } UA_String;
//////**
/// * Guid
/// * ^^^^
/// * A 16 byte value that can be used as a globally unique identifier. */
// typedef struct {
//     UA_UInt32 data1;
//     UA_UInt16 data2;
//     UA_UInt16 data3;
//     UA_Byte data4[8];
// } UA_Guid;
// typedef uint16_t UA_UInt16;
// #define UA_UINT16_MIN 0
// #define UA_UINT16_MAX 65535
static UA_NodeId* UA_NodeId_create_arena(memory_arena* arena) {
    UA_NodeId* node = (UA_NodeId*)arena_alloc(arena, sizeof(UA_NodeId), alignof(UA_NodeId));
    return node;
}
static void UA_NodeId_copy_arena(memory_arena* arena, const UA_NodeId* src, UA_NodeId* dst) {
    memset(dst, 0, sizeof(UA_NodeId));
    dst->namespaceIndex = src->namespaceIndex;
    dst->identifierType = src->identifierType;

    switch (src->identifierType) {
        case UA_NODEIDTYPE_NUMERIC: dst->identifier.numeric = src->identifier.numeric; break;
        case UA_NODEIDTYPE_GUID: dst->identifier.guid = src->identifier.guid; break;
        case UA_NODEIDTYPE_STRING: {
            size_t len = src->identifier.string.length;
            dst->identifier.string.length = len;
            if (len) {
                void* mem = arena_alloc(arena, len, alignof(UA_Byte));
                memcpy(mem, src->identifier.string.data, len);
                dst->identifier.string.data = (UA_Byte*)mem;
            } else {
                dst->identifier.string.data = NULL;
            }
            break;
        }
        case UA_NODEIDTYPE_BYTESTRING: {
            size_t len = src->identifier.byteString.length;
            dst->identifier.byteString.length = len;
            if (len) {
                void* mem = arena_alloc(arena, len, alignof(UA_Byte));
                memcpy(mem, src->identifier.byteString.data, len);
                dst->identifier.byteString.data = (UA_Byte*)mem;
            } else {
                dst->identifier.byteString.data = NULL;
            }
            break;
        }
        default: break;
    }
}

typedef struct Parser {
    arr_Tokens* tokens;
    size_t current;
    memory_arena* arena;
} Parser;
void parser_init(Parser* parser, arr_Tokens* tokens, memory_arena* arena);
da_UA_NodeId* parser_parse(Parser* p);
