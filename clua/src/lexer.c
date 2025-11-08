#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
Token *lexer_next_token(Lexer *lexer) {
    lexer_skip_whitespace(lexer);

    if (lexer->current >= lexer->length) {
        return token_create(TOKEN_EOF, "", lexer->line, lexer->column);
    }

    char c = lexer_peek(lexer);
    int start = lexer->current;
    int start_col = lexer->column;

    // Numbers
    if (isdigit(c)) {
        while (isdigit(lexer_peek(lexer))) {
            lexer_advance(lexer);
        }

        int length = lexer->current - start;
        char *value = malloc(length + 1);
        strncpy(value, lexer->source + start, length);
        value[length] = '\0';

        Token *token = NULL;
        token = token_create(TOKEN_NUMBER, value, lexer->line, start_col);

        free(value);
        return token;
    }

    // Identifiers and keywords
    if (isalpha(c) || c == '_') {
        while (isalnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_') {
            lexer_advance(lexer);
        }

        int length = lexer->current - start;
        char *value = malloc(length + 1);
        strncpy(value, lexer->source + start, length);
        value[length] = '\0';

        MirTokenType type = TOKEN_IDENTIFIER;
        if (strcmp(value, "help") == 0)
            type = TOKEN_HELP;
        else if (strcmp(value, "save") == 0)
            type = TOKEN_SAVE;
        else if (strcmp(value, "read") == 0)
            type = TOKEN_BROWSE;
        else if (strcmp(value, "add") == 0)
            type = TOKEN_ADD;
        else if (strcmp(value, "undo") == 0)
            type = TOKEN_UNDO;
        else if (strcmp(value, "redo") == 0)
            type = TOKEN_REDO;
        else if (strcmp(value, "ls") == 0)
            type = TOKEN_LIST_DIRS;
        else if (strcmp(value, "con") == 0)
            type = TOKEN_CONNECT;
        else if (strcmp(value, "cd") == 0)
            type = TOKEN_NAVIGATE;
        else if (strcmp(value, "cur") == 0) {
            type = TOKEN_CURRENT;
        } else if (strcmp(value, "disconnect") == 0) {
            type = TOKEN_DISCONNECT;
        } else if (strcmp(value, "set") == 0) {
            type = TOKEN_SET;
        } else if (strcmp(value, "hist") == 0) {
            type = TOKEN_HISTORY;
        } else if (strcmp(value, "state") == 0) {
            type = TOKEN_STATE;
        }

        Token *token = token_create(type, value, lexer->line, start_col);
        free(value);
        return token;
    }
    // Single char tokens
    lexer_advance(lexer);
    switch (c) {
        case '-': return token_create(TOKEN_MINUS, "-", lexer->line, lexer->column - 1);
        case '.': return token_create(TOKEN_DOT, ".", lexer->line, lexer->column - 1);
        case ':': return token_create(TOKEN_COLON, ":", lexer->line, lexer->column - 1);
        case ';': return token_create(TOKEN_SEMICOLON, ";", lexer->line, lexer->column - 1);
        case '=': return token_create(TOKEN_ASSING, "=", lexer->line, lexer->column - 1);
        case '/': return token_create(TOKEN_SLASH, "/", lexer->line, lexer->column - 1);
        default: return token_create(TOKEN_UNKNOWN, "", lexer->line, lexer->column - 1);
    }
}

void token_print(Token *token) {
    printf(
        "Token{type=%s, value='%s', line=%d, col=%d}\n",
        lexer_to_string_tokentype(token->type),
        token->value,
        token->line,
        token->column);
}

const char *lexer_to_string_tokentype(MirTokenType type) {
    switch (type) {
        case TOKEN_NUMBER: return "TOKEN_NUMBER";
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";
        case TOKEN_HELP: return "TOKEN_HELP";
        case TOKEN_BROWSE: return "TOKEN_BROWSE";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_MODIFIER: return "TOKEN_MODIFIER";
        case TOKEN_SAVE: return "TOKEN_SAVE";
        case TOKEN_UNDO: return "TOKEN_UNDO";
        case TOKEN_LIST_DIRS: return "TOKEN_LIST_DIRS";
        case TOKEN_REDO: return "TOKEN_REDO";
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_CURRENT: return "TOKEN_CURRENT";
        case TOKEN_CONNECT: return "TOKEN_CONNECT";
        case TOKEN_DISCONNECT: return "TOKEN_DISCONNECT";
        case TOKEN_UNKNOWN: return "TOKEN_UNKNOWN";
        case TOKEN_COUNT: return "TOKEN_COUNT";
        case TOKEN_ADD: return "TOKEN_ADD";
        case TOKEN_HISTORY: return "TOKEN_HISTORY";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_COLON: return "TOKEN_COLON";
        case TOKEN_SLASH: return "TOKEN_SLASH";
        case TOKEN_NAVIGATE: return "TOKEN_NAVIGATE";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_ASSING: return "TOKEN_ASSING";
        case TOKEN_STATE: return "TOKEN_STATE";
        case TOKEN_SET: return "TOKEN_SET";
    }
    return "UNKNOWN";
}

void token_free(Token *token) {
    if (token) {
        log_de_alloc("[FREEING] - %s", lexer_to_string_tokentype(token->type));
        free(token->value);
        free(token);
    }
}

void lexer_free(Lexer *lexer) {
    if (lexer) {
        log_de_alloc("[FREEING] - lexer");
        free(lexer);
    }
}

char lexer_peek(Lexer *lexer) {
    if (lexer->current >= lexer->length) return '\0';
    return lexer->source[lexer->current];
}

char lexer_advance(Lexer *lexer) {
    if (lexer->current >= lexer->length) return '\0';

    char c = lexer->source[lexer->current++];
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

void lexer_skip_whitespace(Lexer *lexer) {
    while (isspace(lexer_peek(lexer))) {
        lexer_advance(lexer);
    }
}

Token *token_create(MirTokenType type, const char *value, int line, int column) {
    Token *token = malloc(sizeof(Token));
    token->type = type;
    token->value = strdup(value);
    token->line = line;
    token->column = column;
    log_alloc("[CREATED NODE] %s", lexer_to_string_token(token));
    return token;
}

Lexer *lexer_create(const char *source) {
    Lexer *lexer = malloc(sizeof(Lexer));
    lexer->source = source;
    lexer->current = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->length = strlen(source);
    log_alloc("[CREATED LEXER]");
    return lexer;
}
Token **lexer_tokenize_all(const char *input, int *token_count) {
    Lexer *lexer = lexer_create(input);
    if (!lexer) return NULL;

    Token **tokens = NULL;
    int count = 0;
    int capacity = 10;

    tokens = malloc(capacity * sizeof(Token *));

    if (!tokens) {
        lexer_free(lexer);
        return NULL;
    }

    Token *token;
    do {
        token = lexer_next_token(lexer);
        // log_trace("%s",
        // lexer_to_string_token(token));
        if (!token) break;

        if (count >= capacity) {
            capacity *= 2;
            Token **new_tokens = realloc(tokens, capacity * sizeof(Token *));
            if (!new_tokens) {
                lexer_free_tokens(tokens, count);
                lexer_free(lexer);
                return NULL;
            }
            tokens = new_tokens;
        }

        tokens[count++] = token;

    } while (token->type != TOKEN_EOF);

    *token_count = count;
    lexer_free(lexer);
    return tokens;
}

void lexer_free_tokens(Token **tokens, int count) {
    if (!tokens) return;

    for (int i = 0; i < count; i++) {
        token_free(tokens[i]);
    }
    free(tokens);
}

bool lexer_is_command_token(MirTokenType type) {
    return type >= TOKEN_HELP && type < TOKEN_COMMAND_COUNT;
}

bool lexer_is_parameter_token(MirTokenType type) {
    return type >= TOKEN_NUMBER && type < TOKEN_PARAMETER_COUNT;
}

const char *lexer_to_string_token(const Token *token) {
    static char buf[512];
    if (!token) {
        strcpy(buf, "NULL");
        return buf;
    }
    snprintf(
        buf,
        sizeof(buf),
        "Token: [%s] line:%d, column:%d, value:%s",
        lexer_to_string_tokentype(token->type),
        token->line,
        token->column,
        token->value ? token->value : "NULL");
    return buf;
}
