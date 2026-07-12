#include "parser.h"

#include "core/allocator.h"
#include "lexer.h"
#include "log.h"
#include "open62541/types.h"

Token* parser_peek(Parser* p);
Token* parser_advance(Parser* p);
Token* parser_advance_until(Parser* p, TokenType type);
bool parser_match(Parser* p, TokenType type, size_t count);
bool parser_is_at_end(Parser* parser);
Token parser_current(Parser* p);
bool parser_parse_header(Parser* p);
bool parser_match_identifier(Parser* p, Sv wanted);
bool parser_parse_string_field(Parser* p, Sv name, char** out);
ARENA_DEFINE_PUSH_FN(da_UA_NodeId_push, da_UA_NodeId, UA_NodeId, 64)

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
        return (Token){TOKEN_EOF, (Sv){NULL, 0}, 0, 0};
    }
}
bool parser_parse_header(Parser* p) {
    const char* expected[] = {"inputs", "opcua", "group", "nodes"};
    size_t expected_count = sizeof(expected) / sizeof(expected[0]);

    bool found[4] = {false};

    Sv current = parser_current(p).str;

    Sv part;
    while (current.count != 0) {
        part = sv2_chop_by_delim(&current, '.');

        for (size_t i = 0; i < expected_count; i++) {
            if (sv_equal(part, sv_create_from_cstr(expected[i]))) {
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

bool parser_match_identifier(Parser* p, Sv wanted) {
    Token* t = parser_peek(p);
    if (!t || t->type != TOKEN_IDENTIFIER) return false;
    if (!sv_equal(t->str, wanted)) return false;
    p->current++;
    return true;
}
bool parser_parse_string_field(Parser* p, Sv name, char** out) {
    if (!parser_match_identifier(p, name)) return false;
    Token eq = parser_current(p);
    if (!parser_match(p, TOKEN_EQUAL, 1)) {
        printf("invalid format. missing TOKEN_EQUAL at r:%ic:%i\n", eq.line, eq.column);
        return false;
    }
    Token value = parser_current(p);
    parser_advance(p);
    *out = sv_to_cstr_arena(p->arena, value.str);
    if (!*out) {
        printf("failed to allocate memory at r:%ic:%i\n", value.line, value.column);
        exit(EXIT_FAILURE);
    }
    return true;
}

da_UA_NodeId* parser_parse(Parser* p) {
    da_UA_NodeId* nodes =
        (da_UA_NodeId*)arena_alloc(p->arena, sizeof(da_UA_NodeId), alignof(da_UA_NodeId));
    nodes->count = 0;
    nodes->capacity = 0;
    nodes->items = NULL;

    while (!parser_is_at_end(p)) {
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

        parser_parse_string_field(p, SV_LIT("name"), &name);
        parser_parse_string_field(p, SV_LIT("namespace"), &ns);
        parser_parse_string_field(p, SV_LIT("identifier_type"), &identifier_type);
        parser_parse_string_field(p, SV_LIT("identifier"), &identifier);

        int len = snprintf(NULL, 0, "ns=%s;%s=%s", ns, identifier_type, identifier);
        char* nodeid_str = (char*)arena_alloc(p->arena, (size_t)(len + 1), alignof(char));
        snprintf(nodeid_str, (size_t)(len + 1), "ns=%s;%s=%s", ns, identifier_type, identifier);

        UA_NodeId id;
        UA_NodeId_init(&id);
        UA_String ua_str = UA_String_fromChars(nodeid_str);
        UA_StatusCode status = UA_NodeId_parse(&id, ua_str);
        UA_String_clear(&ua_str);

        if (status == UA_STATUSCODE_GOOD) {
            da_UA_NodeId_push(p->arena, nodes, id);
        } else {
            log_error("Failed to parse NodeId '%s': %s", nodeid_str, UA_StatusCode_name(status));
            UA_NodeId_clear(&id);
        }
    }

    return nodes;
}
