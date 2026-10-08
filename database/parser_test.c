#include "parser.h"

#include <assert.h>
#include <core/allocator.h>
#include <core/log.h>
#include <open62541/types.h>
#include <stdio.h>

// parser_parse_header is implemented in parser.c but not exposed in parser.h
bool parser_parse_header(Parser* p);

static arr_Tokens* lex_cfg(memory_arena* arena, char* cfg) {
    Scanner scanner;
    lx_init(&scanner, cfg, arena);
    return lx_tokenize(&scanner);
}

static void test_lexes_double_bracket_header_tokens(void) {
    memory_arena* arena = arena_create(1024 * 1024);

    char cfg[] = "[[inputs.opcua.nodes]]\n";
    arr_Tokens* tokens = lex_cfg(arena, cfg);

    assert(tokens != NULL);
    assert(tokens->count >= 6);

    // Expected: [ [ IDENT ] ] EOF
    assert(tokens->items[0].type == TOKEN_LBRACKET);
    assert(tokens->items[1].type == TOKEN_LBRACKET);
    assert(tokens->items[2].type == TOKEN_IDENTIFIER);
    assert(tokens->items[3].type == TOKEN_RBRACKET);
    assert(tokens->items[4].type == TOKEN_RBRACKET);
    assert(tokens->items[5].type == TOKEN_EOF);

    arena_free(arena);
}

static void test_parse_header_from_lexed_tokens(void) {
    memory_arena* arena = arena_create(1024 * 1024);

    char cfg[] = "[[inputs.opcua.nodes]]\n";
    arr_Tokens* tokens = lex_cfg(arena, cfg);

    Parser p;
    parser_init(&p, tokens, arena);

    // Move parser to header identifier token:
    // token[0] = [, token[1] = [, token[2] = "inputs.opcua.nodes"
    p.current = 2;

    bool ok = parser_parse_header(&p);
    assert(ok == true);

    // parser_parse_header advances 3 tokens: IDENT, ], ]
    assert(p.current == 5);
    assert(tokens->items[p.current].type == TOKEN_EOF);

    arena_free(arena);
}

// static void test_test(void) {
//     const char* nodeid_str = "ns=2;s=Machine.Temp";
//     UA_NodeId id;
//     UA_NodeId_init(&id);
//     // TODO: make own ua nodeid creator to not use malloc
//     UA_String ua_str = UA_String_fromChars(nodeid_str);
//     UA_StatusCode status = UA_NodeId_parse(&id, ua_str);
//     UA_String_clear(&ua_str);
//
//     da_ParsedNode arr = {0};
//     ParsedNode node = (ParsedNode){"test", id, 100, true};
//     da_append(&arr, node);
//     log_info("lol");
//     UA_NodeId_clear(&id);
//     log_info("lol");
// }

static void test_parse_one_string_node(void) {
    memory_arena* arena = arena_create(1024 * 1024);
    char cfg[] =
        "[[inputs.opcua.nodes]]\n"
        "name = \"Temp\"\n"
        "namespace = \"2\"\n"
        "identifier_type = \"s\"\n"
        "identifier = \"Machine.Temp\"\n";

    da_ParsedNode* nodes = parser_parse(arena, cfg);

    assert(nodes != NULL);
    assert(nodes->count == 1);
    assert(nodes->items[0].nodeId.namespaceIndex == 2);
    assert(nodes->items[0].nodeId.identifierType == UA_NODEIDTYPE_STRING);
    assert(nodes->items[0].polling == 0);
    assert(nodes->items[0].historizing == false);

    arena_free(arena);
}

static void test_parse_one_string_node_with_polling_and_historizing(void) {
    memory_arena* arena = arena_create(1024 * 1024);
    char cfg[] =
        "[[inputs.opcua.nodes]]\n"
        "name = \"var10\"\n"
        "namespace = \"1\"\n"
        "identifier_type = \"s\"\n"
        "identifier = \"var10\"\n"
        "polling = 100\n"
        "historizing = true\n";

    da_ParsedNode* nodes = parser_parse(arena, cfg);

    assert(nodes != NULL);
    assert(nodes->count == 1);
    assert(nodes->items[0].nodeId.namespaceIndex == 1);
    assert(nodes->items[0].nodeId.identifierType == UA_NODEIDTYPE_STRING);
    assert(nodes->items[0].polling == 100);

    // TODO: add lexing of this. Then add it in parser.
    assert(nodes->items[0].historizing == true);

    arena_free(arena);
}

int main(void) {
    test_lexes_double_bracket_header_tokens();
    test_parse_header_from_lexed_tokens();
    test_parse_one_string_node();
    test_parse_one_string_node_with_polling_and_historizing();
    // test_test();

    puts("parser tests passed");
    return 0;
}
