#include "lexer.h"

#include <ctype.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/allocator.h"
#include "core/log.h"
#include "core/nob.h"

bool lx_isAtEnd(Scanner* scanner);
char lx_peek(Scanner* scanner);
void lx_advance(Scanner* scanner);
Token lx_token_create_scanner(Scanner* scanner, TokenType type);
Token lx_token_create(TokenType type, const char* data, size_t lenght, int line, int column);
Token lx_scanToken(Scanner* scanner);

#define C_RESET "\x1b[0m"

#define C_BLACK "\x1b[30m"
#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_MAGENTA "\x1b[35m"
#define C_CYAN "\x1b[36m"
#define C_WHITE "\x1b[37m"

#define C_BOLD "\x1b[1m"
#define C_DIM "\x1b[2m"
#define C_UNDER "\x1b[4m"

// Optional bright variants
#define C_BRED "\x1b[91m"
#define C_BGREEN "\x1b[92m"
#define C_BYELLOW "\x1b[93m"
#define C_BBLUE "\x1b[94m"
#define C_BMAGENTA "\x1b[95m"
#define C_BCYAN "\x1b[96m"

#define LX_TOKEN_FMT_BASE                                                                  \
    C_RESET "type=" C_GREEN "%-18s" C_RESET " line=" C_YELLOW "%-5d" C_RESET " col=" C_RED \
            "%-5d" C_RESET

#define LX_TOKEN_FMT_TEXT LX_TOKEN_FMT_BASE " text=\"" C_GREEN SV_Fmt C_RESET "\""
#define LX_TOKEN_FMT_INT LX_TOKEN_FMT_BASE " value=" C_GREEN "%lld" C_RESET
#define LX_TOKEN_FMT_FLOAT LX_TOKEN_FMT_BASE " value=" C_GREEN "%f" C_RESET
#define LX_TOKEN_FMT_DEFAULT LX_TOKEN_FMT_BASE

void lx_init(Scanner* scanner, const char* source, memory_arena* _arena) {
    scanner->sv = sv_from_cstr(source);
    scanner->start = 0;
    scanner->current = 0;
    scanner->line_start = 0;
    scanner->line = 1;
    scanner->arena = _arena;
    scanner->initialAlloc = 128;
}

void lx_init_sv(Scanner* scanner, String_View source, memory_arena* _arena) {
    scanner->sv = source;
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

Token lx_token_create_string(
    TokenType type, const char* data, size_t lenght, int line, int column) {
    Token t;
    t.type = type;
    t.as.string.data = data;
    t.as.string.count = lenght;
    t.line = line;
    t.column = column;
    return t;
}
Token lx_token_create_integer(String_View text, int line, int column) {
    Token t = {
        .type = TOKEN_INTEGER,
        .line = line,
        .column = column,
    };

    char buffer[64];

    if (text.count >= sizeof(buffer)) {
        t.type = TOKEN_ERROR;
        t.as.i = 0;
        return t;
    }

    memcpy(buffer, text.data, text.count);
    buffer[text.count] = '\0';

    char* end;
    errno = 0;

    int64_t value = strtoll(buffer, &end, 10);

    if (errno == ERANGE || *end != '\0') {
        t.type = TOKEN_ERROR;
        t.as.i = 0;
        return t;
    }

    t.as.i = value;
    return t;
}

Token lx_token_create_float(String_View text, int line, int column) {
    Token t = {
        .type = TOKEN_FLOAT,
        .line = line,
        .column = column,
    };

    char buffer[64];

    if (text.count >= sizeof(buffer)) {
        t.type = TOKEN_ERROR;
        t.as.f = 0.0f;
        return t;
    }

    memcpy(buffer, text.data, text.count);
    buffer[text.count] = '\0';

    char* end;
    errno = 0;

    float value = strtof(buffer, &end);

    if (errno == ERANGE || *end != '\0') {
        t.type = TOKEN_ERROR;
        t.as.f = 0.0f;
        return t;
    }

    t.as.f = value;
    return t;
}

Token lx_token_create_scanner(Scanner* scanner, TokenType type) {
    int col = (int)(scanner->start - scanner->line_start) + 1;

    String_View text = {
        .data = scanner->sv.data + scanner->start,
        .count = scanner->current - scanner->start,
    };

    switch (type) {
        case TOKEN_INTEGER: return lx_token_create_integer(text, scanner->line, col);

        case TOKEN_FLOAT: return lx_token_create_float(text, scanner->line, col);

        case TOKEN_TEXT:
            return lx_token_create_string(type, text.data, text.count, scanner->line, col);

        default: return lx_token_create_string(type, text.data, text.count, scanner->line, col);
    }
}

static inline bool lx_is_identifier_delim(char c) {
    return c == '[' || c == ']' || c == '\n' || c == ' ' || c == '\t' || c == '\r';
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

        if (c == '{') {
            lx_advance(scanner);
            return lx_token_create_scanner(scanner, TOKEN_LBRACE);
        }

        if (c == '}') {
            lx_advance(scanner);
            return lx_token_create_scanner(scanner, TOKEN_RBRACE);
        }

        if (c == '=') {
            lx_advance(scanner);
            return lx_token_create_scanner(scanner, TOKEN_EQUAL);
        }
        if (c == ':') {
            lx_advance(scanner);
            return lx_token_create_scanner(scanner, TOKEN_COLON);
        }
        if (c == ',') {
            lx_advance(scanner);
            return lx_token_create_scanner(scanner, TOKEN_COMMA);
        }

        if (isdigit(c)) {
            bool isFloat = false;
            while (!lx_isAtEnd(scanner)) {
                c = lx_peek(scanner);
                if (isdigit(c)) {
                    lx_advance(scanner);
                } else if (c == '.' && !isFloat) {
                    isFloat = true;
                    lx_advance(scanner);
                } else {
                    break;
                }
            }
            return lx_token_create_scanner(scanner, isFloat ? TOKEN_FLOAT : TOKEN_INTEGER);
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
        //
        // if (isdigit(c)) {
        //     return lx_token_create_scanner(scanner, TOKEN_FLOAT);
        // }

        while (!lx_isAtEnd(scanner)) {
            c = lx_peek(scanner);
            if (lx_is_identifier_delim(c)) {
                break;
            }
            lx_advance(scanner);
        }

        return lx_token_create_scanner(scanner, TOKEN_IDENTIFIER);
    }

    scanner->start = scanner->current;
    return lx_token_create_scanner(scanner, TOKEN_EOF);
}

// Macro to avoid mistakes miss typing the return value.
#define LX_TOKEN_CASE(e) \
    case e: return #e;
const char* lx_tokenTypeToString(TokenType type) {
    switch (type) {
        LX_TOKEN_CASE(TOKEN_EOF)
        LX_TOKEN_CASE(TOKEN_LBRACKET)
        LX_TOKEN_CASE(TOKEN_RBRACKET)
        LX_TOKEN_CASE(TOKEN_LBRACE)
        LX_TOKEN_CASE(TOKEN_RBRACE)
        LX_TOKEN_CASE(TOKEN_IDENTIFIER)
        LX_TOKEN_CASE(TOKEN_TEXT)
        LX_TOKEN_CASE(TOKEN_EQUAL)
        LX_TOKEN_CASE(TOKEN_COLON)
        LX_TOKEN_CASE(TOKEN_COMMA)
        LX_TOKEN_CASE(TOKEN_FLOAT)
        LX_TOKEN_CASE(TOKEN_INTEGER)
        LX_TOKEN_CASE(TOKEN_ERROR)
    }

    return "TOKEN_UNKNOWN";
}

int lx_tokenFormat(char* buf, size_t size, Token t) {
    if (t.type == TOKEN_INTEGER) {
        return snprintf(
            buf,
            size,
            LX_TOKEN_FMT_INT,
            lx_tokenTypeToString(t.type),
            t.line,
            t.column,
            (long long)t.as.i);

    } else if (t.type == TOKEN_FLOAT) {
        return snprintf(
            buf,
            size,
            LX_TOKEN_FMT_FLOAT,
            lx_tokenTypeToString(t.type),
            t.line,
            t.column,
            (double)t.as.f);

    } else {
        return snprintf(
            buf,
            size,
            LX_TOKEN_FMT_TEXT,
            lx_tokenTypeToString(t.type),
            t.line,
            t.column,
            SV_Arg(t.as.string));
    }
    // switch (t.type) {
    //     case TOKEN_TEXT:
    //     case TOKEN_IDENTIFIER:
    //     case TOKEN_LBRACKET:
    //     case TOKEN_RBRACKET:
    //     case TOKEN_COLON:
    //     case TOKEN_EQUAL:
    //
    //     case TOKEN_INTEGER:
    //
    //     default:
    //         return snprintf(
    //             buf, size, LX_TOKEN_FMT_DEFAULT, lx_tokenTypeToString(t.type),
    //             t.line, t.column);
    // }
}

const char* lx_tokenToStringArena(Token t, memory_arena* arena) {
    int len = lx_tokenFormat(NULL, 0, t);
    if (len < 0) {
        log_fatal("Failed to format token");
        return NULL;
    }

    char* buf = (char*)arena_alloc(arena, (size_t)(len + 1), alignof(char));
    if (!buf) {
        log_fatal("Arena out of memory! returning buffer early");
        return NULL;
    }

    lx_tokenFormat(buf, (size_t)(len + 1), t);
    return buf;
}

const char* lx_tokensToStringArena(arr_Tokens* tokens, memory_arena* arena) {
    size_t total = 0;

    for (size_t i = 0; i < tokens->count; i++) {
        int len = lx_tokenFormat(NULL, 0, tokens->items[i]);
        if (len < 0) return NULL;
        total += (size_t)len + 1;  // + newline
    }

    char* buf = (char*)arena_alloc(arena, total + 1, alignof(char));
    if (!buf) {
        log_fatal("Arena out of memory!");
        return NULL;
    }

    size_t offset = 0;
    for (size_t i = 0; i < tokens->count; i++) {
        int len = lx_tokenFormat(buf + offset, total + 1 - offset, tokens->items[i]);
        if (len < 0) return NULL;
        offset += (size_t)len;
        buf[offset++] = '\n';
    }

    buf[offset] = '\0';
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
        da_arena_append(scanner->arena, tokens, t);
        if (t.type == TOKEN_EOF) break;
    }
    return tokens;
}
