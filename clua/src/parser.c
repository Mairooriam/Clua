
#include "parser.h"

#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "debug_buf.h"
#include "lexer.h"
#include "log.h"

AST* parser_parse(Parser* parser) {
    AST* ast = ast_create();
    if (!ast) return NULL;

    while (!parser_is_at_end(parser)) {
        ASTNode* node = NULL;

        if ((node = parse_navigate(parser)) || (node = parse_ip(parser)) ||
            (node = parse_parameter(parser)) || (node = parse_connect(parser)) ||
            (node = parse_list_dirs(parser)) || (node = parse_save(parser)) ||
            (node = parse_state(parser)) || (node = parse_set(parser))
            /* add more explicit command parsers here */
        ) {
            ast_add_statement(ast, node);
        } else {
            if (parser->tokens[parser->current]->type != TOKEN_EOF) {
                log_warn("[PARSE] - Not handled token.");
            }
            parser->current++;
        }
    }

    char* buf = dbg_getbuf(0);
    size_t bufsize = dbg_buf_size();
    size_t offset = 0;
    ast_to_string_ast(buf, bufsize, &offset, ast);
    log_trace("[FINAL AST] - %s", buf);
    return ast;
}

ASTNode* parse_range(Parser* parser) {
    if (parser->current + 2 < parser->token_count &&
        parser->tokens[parser->current]->type == TOKEN_NUMBER &&
        parser->tokens[parser->current + 1]->type == TOKEN_MINUS &&
        parser->tokens[parser->current + 2]->type == TOKEN_NUMBER) {
        int start = atoi(parser->tokens[parser->current]->value);
        int end = atoi(parser->tokens[parser->current + 2]->value);
        parser->current += 3;  // Consume tokens
        return ast_create_node_range(start, end);
    }
    return NULL;
}
ASTNode* parse_parameter(Parser* parser) {
    Token* token = parser_current_token(parser);
    ASTParameterData data = {0};
    if (token->type == TOKEN_IDENTIFIER) {
        data.type = AST_PARAM_TYPE_STRING;
        data.value.s = strdup(token->value);
        parser_advance(parser);
        return ast_create_node_parameter(data);
    } else if (token->type == TOKEN_NUMBER) {
        data.type = AST_PARAM_TYPE_INT;
        data.value.i = atoi(token->value);
        parser_advance(parser);
        return ast_create_node_parameter(data);
    }
    return NULL;
}
ASTNode* parse_ip(Parser* parser) {
    if (parser->current + 6 < parser->token_count &&
        parser->tokens[parser->current]->type == TOKEN_NUMBER &&
        parser->tokens[parser->current + 1]->type == TOKEN_DOT &&
        parser->tokens[parser->current + 2]->type == TOKEN_NUMBER &&
        parser->tokens[parser->current + 3]->type == TOKEN_DOT &&
        parser->tokens[parser->current + 4]->type == TOKEN_NUMBER &&
        parser->tokens[parser->current + 5]->type == TOKEN_DOT &&
        parser->tokens[parser->current + 6]->type == TOKEN_NUMBER) {
        char ip_str[32];
        snprintf(
            ip_str,
            sizeof(ip_str),
            "%s.%s.%s.%s",
            parser->tokens[parser->current]->value,
            parser->tokens[parser->current + 2]->value,
            parser->tokens[parser->current + 4]->value,
            parser->tokens[parser->current + 6]->value);
        parser->current += 7;  // Consume tokens
        return ast_create_node_ip(ip_str);
    }
    return NULL;
}

ASTNode* parse_state(Parser* parser) {
    if (parser_current_token(parser)->type == TOKEN_STATE) {
        parser_advance(parser);
        return ast_create_node_state();
    }
    return NULL;
}

ASTNode* parse_set(Parser* parser) {
    if (parser_current_token(parser)->type == TOKEN_SET) {
        ASTNode* parent = ast_create_node_set();
        parser_advance(parser);

        ASTNode* key = parse_parameter(parser);
        if (!key) {
            log_warn("[PARSE SET] - No key! not valid set command\n");
            return NULL;
        }
        ast_add_child(parent, key);

        // Consume all remaining tokens into one value string
        char* value_str = NULL;
        size_t len = 0;
        while (!parser_is_at_end(parser)) {
            Token* token = parser_current_token(parser);
            if (token->type == TOKEN_EOF) break;

            size_t token_len = strlen(token->value);
            value_str = realloc(value_str, len + token_len + 1);  // +2 for space/null
            strcpy(value_str + len, token->value);
            len += token_len;
            parser_advance(parser);
        }

        if (value_str) {
            ASTParameterData data = {0};
            data.type = AST_PARAM_TYPE_STRING;
            data.value.s = value_str;
            ASTNode* value_node = ast_create_node_parameter(data);
            ast_add_child(parent, value_node);
        }

        return parent;
    }
    

    return NULL;
}

ASTNode* parse_save(Parser* parser) {
    if (parser->tokens[parser->current]->type == TOKEN_SAVE) {
        ASTNode* node = ast_create_node_save();
        parser->current++;
        ASTNode* range = parse_range(parser);
        if (range) {
            ast_add_child(node, range);
        }
        ASTNode* param = parse_parameter(parser);
        if (param) {
            ast_add_child(node, param);
        }
        return node;
    }
    return NULL;
}
ASTNode* parse_navigate(Parser* parser) {
    // Check for "navigate .." (up)
    if (parser->current + 2 < parser->token_count &&
        parser->tokens[parser->current]->type == TOKEN_NAVIGATE &&
        parser->tokens[parser->current + 1]->type == TOKEN_DOT &&
        parser->tokens[parser->current + 2]->type == TOKEN_DOT) {
        parser->current += 3;
        return ast_create_node_navigate_up();  // TODO: combine navigate into one AST with types?
    }

    // Check for "navigate <number>" (down)
    if (parser->current + 1 < parser->token_count &&
        parser->tokens[parser->current]->type == TOKEN_NAVIGATE &&
        parser->tokens[parser->current + 1]->type == TOKEN_NUMBER) {
        ASTNode* node = ast_create_node_navigate_down();
        ASTParameterData data = {0};
        data.type = AST_PARAM_TYPE_INT;
        data.value.i = atoi(parser->tokens[parser->current + 1]->value);
        ASTNode* param = ast_create_node_parameter(data);
        ast_add_child(node, param);
        parser->current += 2;

        ASTNode* nodeid = parse_nodeid(parser);
        if (nodeid) {
            ast_add_child(node, nodeid);
        }
        return node;
    }

    // Check for "navigate <identifier>" (to)
    if (parser->current < parser->token_count &&
        parser->tokens[parser->current]->type == TOKEN_NAVIGATE) {
        parser->current += 1;
        ASTNode* node =
            ast_create_node_navigate_to();  // TODO: combine navigate into one AST with types?

        ASTNode* child = parse_nodeid(parser);
        if (child) {
            ast_add_child(node, child);
        }
        return node;
    }

    return NULL;
}

ASTNode* parse_connect(Parser* parser) {
    if (parser->current < parser->token_count &&
        parser->tokens[parser->current]->type == TOKEN_CONNECT) {
        ASTNode* connect_node = ast_create_node_connect();
        parser->current++;

        ASTNode* ip_node = parse_ip(parser);
        if (ip_node) {
            ast_add_child(connect_node, ip_node);
        }

        return connect_node;
    }
    return NULL;
}
ASTNode* parse_list_dirs(Parser* parser) {
    if (parser->current < parser->token_count &&
        parser->tokens[parser->current]->type == TOKEN_LIST_DIRS) {
        parser->current++;
        ASTListDirsData data = {0};
        data.idx = -1;

        if (parser->current < parser->token_count &&
            parser->tokens[parser->current]->type == TOKEN_NUMBER) {
            data.idx = atoi(parser->tokens[parser->current]->value);
            parser->current++;
        }

        ASTNode* list_dirs_node = ast_create_node_list_dirs(data);

        if (parser->current < parser->token_count &&
            parser->tokens[parser->current]->type == TOKEN_IDENTIFIER) {
            ASTNode* nodeid_node = parse_nodeid(parser);
            if (nodeid_node) {
                ast_add_child(list_dirs_node, nodeid_node);
            } else {
                ASTNode* param_node = parse_parameter(parser);
                if (param_node) {
                    ast_add_child(list_dirs_node, param_node);
                }
            }
        }

        return list_dirs_node;
    }
    return NULL;
}

// TODO: add flag to lexer -n or some other flag to specify start of nodeid? easier parsing
ASTNode* parse_nodeid(Parser* parser) {
    // Parse "ns=<number>;"
    if (parser->current + 3 < parser->token_count &&
        parser->tokens[parser->current]->type == TOKEN_IDENTIFIER &&
        strcmp(parser->tokens[parser->current]->value, "ns") == 0 &&
        parser->tokens[parser->current + 1]->type == TOKEN_ASSING &&
        parser->tokens[parser->current + 2]->type == TOKEN_NUMBER &&
        parser->tokens[parser->current + 3]->type == TOKEN_SEMICOLON) {
        ASTNodeIdData data = {0};
        data.ns = atoi(parser->tokens[parser->current + 2]->value);
        parser->current += 4;  // Consume "ns=<number>;"

        // Now parse the identifier part: "i=<number>" or "s=<string>"
        if (parser->current + 1 < parser->token_count &&
            parser->tokens[parser->current]->type == TOKEN_IDENTIFIER &&
            parser->tokens[parser->current + 1]->type == TOKEN_ASSING) {
            Token* identifierToken = parser->tokens[parser->current];
            parser->current += 2;  // Consume "<identifier>="

            if (strcmp(identifierToken->value, "i") == 0) {
                // Expect a number
                if (parser->current < parser->token_count &&
                    parser->tokens[parser->current]->type == TOKEN_NUMBER) {
                    data.type = NODEID_IDENTIFIER_INT;
                    data.identifier.i = atoi(parser->tokens[parser->current]->value);
                    parser->current += 1;
                } else {
                    log_error("Expected number after 'i=' in nodeid");
                    return NULL;
                }
            } else if (strcmp(identifierToken->value, "s") == 0) {
                // Collect remaining tokens as string until EOF
                data.type = NODEID_IDENTIFIER_STRING;
                char* string_value = NULL;
                size_t string_len = 0;
                while (parser->current < parser->token_count &&
                       parser->tokens[parser->current]->type != TOKEN_EOF) {
                    const char* token_value = parser->tokens[parser->current]->value;
                    size_t token_len = strlen(token_value);
                    string_value = realloc(string_value, string_len + token_len + 1);
                    strcpy(string_value + string_len, token_value);
                    string_len += token_len;
                    parser->current += 1;
                }
                if (string_value) {
                    string_value[string_len] = '\0';
                    data.identifier.s = string_value;
                } else {
                    data.identifier.s = strdup("");
                }
            } else {
                log_error(
                    "Unknown identifier '%s' in nodeid, expected 'i' or 's'",
                    identifierToken->value);
                return NULL;
            }

            // Construct nodeid_str
            char nodeid_str[256];  // Increased buffer size for longer strings
            if (data.type == NODEID_IDENTIFIER_INT) {
                snprintf(nodeid_str, sizeof(nodeid_str), "ns=%d;i=%d", data.ns, data.identifier.i);
            } else {
                snprintf(nodeid_str, sizeof(nodeid_str), "ns=%d;s=%s", data.ns, data.identifier.s);
            }
            data.str = strdup(nodeid_str);

            return ast_create_node_nodeId(data);
        } else {
            log_error("Expected 'i=' or 's=' after 'ns=<number>;' in nodeid");
            return NULL;
        }
    }
    return NULL;
}
Token* parser_current_token(Parser* parser) {
    if (!parser_is_at_end(parser)) {
        return parser->tokens[parser->current];
    }
    return NULL;
}

Token* parser_advance(Parser* parser) {
    if (parser->current == parser->token_count) {
        return NULL;
    }
    parser->current++;
    return parser->tokens[parser->current];
}

bool parser_is_at_end(Parser* parser) {
    if (parser->current == parser->token_count) {
        return true;
    }
    return false;
}

bool parser_match(Parser* parser, MirTokenType type) {
    if (parser->tokens[parser->current]->type == type) {
        return true;
    }
    return false;
}

Parser* parser_create(Token** tokens, size_t token_count) {
    Parser* parser = malloc(sizeof(Parser));
    if (!parser) return NULL;

    parser->token_count = token_count;
    parser->tokens = tokens;
    parser->current = 0;
    log_alloc("[CREATED] - parser");
    return parser;
}

void parser_free(Parser* parser) {
    if (!parser) return;

    log_de_alloc("[FREEING] - parser");
    free(parser);
}
