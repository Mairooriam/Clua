#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "ast.h"
#include "lexer.h"
typedef struct {
    Token** tokens;
    size_t token_count;
    size_t current;
} Parser;

// Allocation
Parser* parser_create(Token** tokens, size_t token_count);
void parser_free(Parser* parser);

// main parse
AST* parser_parse(Parser* parser);
Token* parser_current_token(Parser* parser);
Token* parser_advance(Parser* parser);
bool parser_is_at_end(Parser* parser);

// Helpers
ASTNode* parse_parameter(Parser* parser);
ASTNode* parse_range(Parser* parser);
ASTNode* parse_ip(Parser* parser);
ASTNode* parse_connect(Parser* parser);
ASTNode* parse_list_dirs(Parser* parser);
ASTNode* parse_nodeid(Parser* parser);
ASTNode* parse_navigate(Parser* parser);
ASTNode* parser_save(Parser* parser);
ASTNode* parse_state(Parser* parser);
ASTNode* parse_set(Parser* parser);
// ASTNode* parse_navigate_down(Parser* parser);  // TODO: combine to single nagivate
// ASTNode* parse_navigate_up(Parser* parser);    // TODO: combine to single navigate
// ASTNode* parse_navigate_to(Parser* parser);    // TODO: combine to single navigate
ASTNode* parse_save(Parser* parser);

bool parser_match(Parser* parser, MirTokenType type);

// AST Creation
