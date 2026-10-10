#include "parser.h"

#include <stdint.h>

#include "core/allocator.h"
#include "core/lexer.h"
#include "core/log.h"
#include "core/string.h"
#include "open62541/types.h"
#define SV_ARG(sv) ((int)sv.count), sv.data))
#define SV_LIT(s) ((String_View){.data = (s), .count = sizeof(s) - 1})

da_ParsedNodes* _parser_parse(Parser* p);
Token* parser_peek(Parser* p);
Token* parser_advance(Parser* p);
Token* parser_advance_until(Parser* p, TokenType type);
bool parser_match(Parser* p, TokenType type, size_t count);
bool parser_is_at_end(Parser* parser);
Token parser_current(Parser* p);
bool parser_parse_header(Parser* p);
bool parser_match_identifier(Parser* p, String_View wanted);
bool parser_parse_string_field(Parser* p, String_View name, char** out);
bool parser_parse_integer_field(Parser* p, String_View name, int64_t* out);
bool parser_parse_boolean_field(Parser* p, String_View name, bool* out);
bool parser_parse_type_field(Parser* p, String_View name, ValueType* out);

// Macro to avoid mistakes miss typing the return value.
#define PARSER_TOKEN_CASE(e) \
    case e: return #e;

const char* parser_value_type_to_string(ValueType type) {
    switch (type) {
        PARSER_TOKEN_CASE(VT_SENTINEL)
        PARSER_TOKEN_CASE(VT_FLOAT)
        PARSER_TOKEN_CASE(VT_INT)
        PARSER_TOKEN_CASE(VT_STRING)
        PARSER_TOKEN_CASE(VT_UNSUPPORTED)
    }

    return "TOKEN_UNKNOWN";
}

// TODO: add float when needed
//  bool parser_parse_float_field(Parser* p, String_View name, int64_t* out);

// TODO: this is at wrong place...
void UA_NodeId_copy_arena(memory_arena* arena, const UA_NodeId* src, UA_NodeId* dst) {
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

void parser_init(Parser* parser, arr_Tokens* tokens, memory_arena* arena) {
    parser->tokens = tokens;
    parser->current = 0;
    parser->arena = arena;
}

Token* parser_peek(Parser* p) {
    if (p->current >= p->tokens->count) return NULL;
    return &p->tokens->items[p->current];
}

Token* parser_advance(Parser* p) {
    Token* t = parser_peek(p);
    if (t) p->current++;
    return t;
}

Token* parser_advance_until(Parser* p, TokenType type) {
    Token* t = parser_peek(p);
    while (t != NULL) {
        if (t->type != type) {
            p->current++;
        } else {
            return t;
        }

        parser_advance(p);
        t = parser_peek(p);
    }
    return t;
}

bool parser_match(Parser* p, TokenType type, size_t count) {
    for (size_t i = 0; i < count; i++) {
        Token* t = parser_peek(p);
        if (!t || t->type != type) return false;
        p->current++;
    }
    return true;
}
bool parser_is_at_end(Parser* parser) {
    if (parser->current >= parser->tokens->count) {
        return true;
    } else {
        return false;
    }
}
Token parser_current(Parser* p) {
    if (p->current < p->tokens->count) {
        return p->tokens->items[p->current];
    } else {
        return (Token){TOKEN_EOF, (String_View){NULL, 0}, 0, 0};
    }
}
// TODO: add tests for this?
bool parser_parse_header(Parser* p) {
    // const char* expected[] = {"inputs", "opcua", "group", "nodes"};
    const char* expected[] = {"inputs", "opcua", "nodes"};

    size_t expected_count = sizeof(expected) / sizeof(expected[0]);

    bool found[4] = {false};

    String_View current = parser_current(p).as.string;

    String_View part = {0};
    while (current.count != 0) {
        part = sv_chop_by_delim(&current, '.');

        for (size_t i = 0; i < expected_count; i++) {
            if (sv_eq(part, sv_from_cstr(expected[i]))) {
                found[i] = true;
            }
        }
    }

    // TODO: improve error messaging. only find first error and returns that.
    for (size_t i = 0; i < expected_count; i++) {
        if (!found[i]) {
            printf("Missing field: %s\n", expected[i]);
            return false;
        }
    }

    parser_advance(p);
    parser_advance(p);
    parser_advance(p);
    return true;
}

bool parser_match_identifier(Parser* p, String_View wanted) {
    Token* t = parser_peek(p);
    if (!t || t->type != TOKEN_IDENTIFIER) return false;
    if (!sv_eq(t->as.string, wanted)) return false;
    p->current++;
    return true;
}
bool parser_parse_string_field(Parser* p, String_View name, char** out) {
    if (!parser_match_identifier(p, name)) return false;
    Token eq = parser_current(p);
    if (!parser_match(p, TOKEN_EQUAL, 1)) {
        log_error("invalid format. missing TOKEN_EQUAL at r:%ic:%i", eq.line, eq.column);
        return false;
    }
    Token value = parser_current(p);
    parser_advance(p);

    if (value.type != TOKEN_TEXT) {
        log_error(
            "Expected TokenType to be Text, but it is %s. r:%ic:%i",
            lx_tokenTypeToString(value.type),
            value.line,
            value.column);
        return false;
    }

    *out = sv_to_cstr_arena(p->arena, value.as.string);
    if (!*out) {
        log_error("failed to allocate memory at r:%ic:%i", value.line, value.column);
        exit(EXIT_FAILURE);
    }
    return true;
}
bool parser_parse_integer_field(Parser* p, String_View name, int64_t* out) {
    if (!parser_match_identifier(p, name)) return false;
    Token eq = parser_current(p);
    if (!parser_match(p, TOKEN_EQUAL, 1)) {
        log_error("invalid format. missing TOKEN_EQUAL at r:%ic:%i", eq.line, eq.column);
        return false;
    }
    Token value = parser_current(p);
    parser_advance(p);

    if (value.type != TOKEN_INTEGER) {
        log_error(
            "Expected TokenType to be Integer, but it is %s. r:%ic:%i",
            lx_tokenTypeToString(value.type),
            value.line,
            value.column);
        return false;
    }

    *out = value.as.i;
    return true;
}
bool parser_parse_boolean_field(Parser* p, String_View name, bool* out) {
    if (!parser_match_identifier(p, name)) return false;
    Token eq = parser_current(p);
    if (!parser_match(p, TOKEN_EQUAL, 1)) {
        log_error("invalid format. missing TOKEN_EQUAL at r:%ic:%i", eq.line, eq.column);
        return false;
    }
    Token value = parser_current(p);
    parser_advance(p);

    if (value.type != TOKEN_BOOLEAN) {
        log_error(
            "Expected TokenType to be Integer, but it is %s. r:%ic:%i",
            lx_tokenTypeToString(value.type),
            value.line,
            value.column);
        return false;
    }

    *out = value.as.b;
    return true;
}
bool parser_parse_type_field(Parser* p, String_View name, ValueType* out) {
    if (!parser_match_identifier(p, name)) return false;
    Token eq = parser_current(p);
    if (!parser_match(p, TOKEN_EQUAL, 1)) {
        log_error("invalid format. missing TOKEN_EQUAL at r:%ic:%i", eq.line, eq.column);
        return false;
    }
    Token value = parser_current(p);
    parser_advance(p);

    if (value.type != TOKEN_TEXT) {
        log_error(
            "Expected TokenType to be Integer, but it is %s. r:%ic:%i",
            lx_tokenTypeToString(value.type),
            value.line,
            value.column);
        return false;
    }

    // INT
    if (sv_eq(value.as.string, SV_LIT("INT"))) {
        *out = VT_INT;
    } else if (sv_eq(value.as.string, SV_LIT("FLOAT"))) {
        *out = VT_FLOAT;
    } else if (sv_eq(value.as.string, SV_LIT("STRING"))) {
        *out = VT_STRING;
    } else {
        *out = VT_UNSUPPORTED;
        return false;
    }
    return true;
}

da_ParsedNodes* _parser_parse(Parser* p) {
    // da_ParsedNode* nodes =
    //     (da_ParsedNode*)arena_alloc(p->arena, sizeof(da_ParsedNode), alignof(da_ParsedNode));
    da_ParsedNodes* nodes = arena_calloc_single(p->arena, da_ParsedNodes);

    while (!parser_is_at_end(p)) {
        // Finds the start of an entry
        if (!parser_match(p, TOKEN_LBRACKET, 2)) {
            parser_advance(p);
            continue;
        }
        if (!parser_parse_header(p)) {
            parser_advance_until(p, TOKEN_LBRACKET);
            continue;
        }

        char* name = NULL;
        char* ns = NULL;
        char* identifier_type = NULL;
        char* identifier = NULL;
        int64_t polling = 0;
        bool historizing = false;
        ValueType type = VT_UNSUPPORTED;

        // TODO: add if and logging to errors.
        parser_parse_string_field(p, SV_LIT("name"), &name);
        parser_parse_string_field(p, SV_LIT("namespace"), &ns);
        parser_parse_string_field(p, SV_LIT("identifier_type"), &identifier_type);
        parser_parse_string_field(p, SV_LIT("identifier"), &identifier);
        parser_parse_integer_field(p, SV_LIT("polling"), &polling);
        parser_parse_boolean_field(p, SV_LIT("historizing"), &historizing);
        parser_parse_type_field(p, SV_LIT("type"), &type);

        int len = snprintf(NULL, 0, "ns=%s;%s=%s", ns, identifier_type, identifier);
        char* nodeid_str = (char*)arena_alloc(p->arena, (size_t)(len + 1), alignof(char));
        snprintf(nodeid_str, (size_t)(len + 1), "ns=%s;%s=%s", ns, identifier_type, identifier);

        UA_NodeId id;
        UA_NodeId_init(&id);
        // TODO: make own ua nodeid creator to not use malloc
        UA_String ua_str = UA_String_fromChars(nodeid_str);
        UA_StatusCode status = UA_NodeId_parse(&id, ua_str);
        UA_String_clear(&ua_str);

        if (status != UA_STATUSCODE_GOOD) {
            log_error("Failed to parse NodeId '%s': %s", nodeid_str, UA_StatusCode_name(status));
            UA_NodeId_clear(&id);
            return nodes;
        }

        ParsedNode node = (ParsedNode){
            .name = name, .nodeId = {0}, .polling = polling, .historizing = historizing, .type = type};
        UA_NodeId_copy_arena(p->arena, &id, &node.nodeId);
        da_arena_append(p->arena, nodes, node);
        UA_NodeId_clear(&id);
    }

    return nodes;
}

da_ParsedNodes* parser_parse(memory_arena* arena, char* config) {
    Scanner scanner;
    lx_init(&scanner, config, arena);
    arr_Tokens* tokens = lx_tokenize(&scanner);
    log_trace("tokens: \n%s", lx_tokensToStringArena(tokens, arena));
    Parser parser;
    parser_init(&parser, tokens, arena);
    // TODO:  monitored items currently takes
    // pointer to the nodes so deleting them in temp arena would cause problems
    return _parser_parse(&parser);
}
