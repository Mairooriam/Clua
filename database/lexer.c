#include "lexer.h"

#include <stdalign.h>
#include <stdio.h>

#include "core/allocator.h"
#include "log.h"
#include "nob.h"

bool lx_isAtEnd(Scanner* scanner);
char lx_peek(Scanner* scanner);
void lx_advance(Scanner* scanner);
Token lx_token_create_scanner(Scanner* scanner, TokenType type);
Token lx_token_create(TokenType type, const char* data, size_t lenght, int line, int column);
Token lx_scanToken(Scanner* scanner);
ARENA_DEFINE_PUSH_FN(arr_tokens_push, arr_Tokens, Token, 128)
#define LX_TOKEN_FMT                                                                       \
    C_RESET "type=" C_GREEN "%-15s" C_RESET " line=" C_YELLOW "%-5d" C_RESET " col=" C_RED \
            "%-5d" C_RESET " text=\"" C_GREEN SV_FTM C_RESET

void lx_init(Scanner* scanner, const char* source, memory_arena* _arena) {
    scanner->sv = sv_create_from_cstr(source);
    ;
    scanner->start = 0;
    scanner->current = 0;
    scanner->line_start = 0;
    scanner->line = 1;
    scanner->arena = _arena;
    scanner->initialAlloc = 128;
}

bool lx_isAtEnd(Scanner* scanner) {
    return scanner->current >= scanner->sv.count;
}

char lx_peek(Scanner* scanner) {
    if (lx_isAtEnd(scanner)) return '\0';
    return scanner->sv.data[scanner->current];
}

void lx_advance(Scanner* scanner) {
    if (!lx_isAtEnd(scanner)) scanner->current++;
}

Token lx_token_create(TokenType type, const char* data, size_t lenght, int line, int column) {
    Token t;
    t.type = type;
    t.str.data = data;
    t.str.count = lenght;
    t.line = line;
    t.column = column;
    return t;
}
Token lx_token_create_scanner(Scanner* scanner, TokenType type) {
    int col = (int)(scanner->start - scanner->line_start) + 1;
    return lx_token_create(
        type,
        scanner->sv.data + scanner->start,
        scanner->current - scanner->start,
        scanner->line,
        col);
}

Token lx_scanToken(Scanner* scanner) {
    while (!lx_isAtEnd(scanner)) {
        char c = lx_peek(scanner);

        if (c == ' ' || c == '\t' || c == '\r') {
            lx_advance(scanner);
            continue;
        }

        if (c == '\n') {
            scanner->line++;
            lx_advance(scanner);
            scanner->line_start = scanner->current;
            continue;
        }
        scanner->start = scanner->current;

        if (c == '[') {
            lx_advance(scanner);
            return lx_token_create_scanner(scanner, TOKEN_LBRACKET);
        }

        if (c == ']') {
            lx_advance(scanner);
            return lx_token_create_scanner(scanner, TOKEN_RBRACKET);
        }

        if (c == '=') {
            lx_advance(scanner);
            return lx_token_create_scanner(scanner, TOKEN_EQUAL);
        }

        if (c == '"') {
            lx_advance(scanner);
            scanner->start = scanner->current;
            while (!lx_isAtEnd(scanner)) {
                c = lx_peek(scanner);
                if (c == '"') {
                    Token t = lx_token_create_scanner(scanner, TOKEN_TEXT);
                    lx_advance(scanner);
                    return t;
                }
                lx_advance(scanner);
            }
            // unterminated string — return whatever was scanned
            return lx_token_create_scanner(scanner, TOKEN_TEXT);
        }

        while (!lx_isAtEnd(scanner)) {
            c = lx_peek(scanner);
            if (c == '[' || c == ']' || c == '\n' || c == ' ' || c == '\t' || c == '\r') {
                break;
            }
            lx_advance(scanner);
        }

        return lx_token_create_scanner(scanner, TOKEN_IDENTIFIER);
    }

    scanner->start = scanner->current;
    return lx_token_create_scanner(scanner, TOKEN_EOF);
}
const char* lx_tokenTypeToString(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return TOSTRING(TOKEN_EOF);
        case TOKEN_LBRACKET: return TOSTRING(TOKEN_LBRACKET);
        case TOKEN_RBRACKET: return TOSTRING(TOKEN_RBRACKET);
        case TOKEN_TEXT: return TOSTRING(TOKEN_TEXT);
        case TOKEN_EQUAL: return TOSTRING(TOKEN_EQUAL);
        case TOKEN_IDENTIFIER: return TOSTRING(TOKEN_IDENTIFIER);

        default: return "unkown";
    }
}
const char* lx_tokenToStringArena(Token t, memory_arena* arena) {
    int len = snprintf(
        NULL, 0, LX_TOKEN_FMT, lx_tokenTypeToString(t.type), t.line, t.column, SV_ARG(t.str));
    char* buf = (char*)arena_alloc(arena, (size_t)(len + 1), alignof(char));
    if (!buf) {
        log_fatal("Arena out of memory! returning buffer early");
        return buf;
    }
    snprintf(
        buf,
        (size_t)(len + 1),
        LX_TOKEN_FMT,
        lx_tokenTypeToString(t.type),
        t.line,
        t.column,
        SV_ARG(t.str));
    return buf;
}

const char* lx_tokensToStringArena(arr_Tokens* tokens, memory_arena* arena) {
    size_t total = 0;
    for (size_t i = 0; i < tokens->count; i++) {
        Token t = tokens->items[i];
        total += (size_t)snprintf(
            NULL,
            0,
            LX_TOKEN_FMT "\n",
            lx_tokenTypeToString(t.type),
            t.line,
            t.column,
            SV_ARG(t.str));
    }
    char* buf = (char*)arena_alloc(arena, total + 1, alignof(char));
    size_t offset = 0;
    for (size_t i = 0; i < tokens->count; i++) {
        Token t = tokens->items[i];
        offset += (size_t)snprintf(
            buf + offset,
            total + 1 - offset,
            LX_TOKEN_FMT "\n",
            lx_tokenTypeToString(t.type),
            t.line,
            t.column,
            SV_ARG(t.str));
    }
    return buf;
}

arr_Tokens* lx_tokenize(Scanner* scanner) {
    arr_Tokens* tokens =
        (arr_Tokens*)arena_alloc(scanner->arena, sizeof(arr_Tokens), alignof(arr_Tokens));

    tokens->capacity = scanner->initialAlloc;
    tokens->count = 0;
    tokens->items =
        (Token*)arena_alloc(scanner->arena, sizeof(Token) * scanner->initialAlloc, alignof(Token));

    for (;;) {
        Token t = lx_scanToken(scanner);
        arr_tokens_push(scanner->arena, tokens, t);
        if (t.type == TOKEN_EOF) break;
    }
    return tokens;
}
