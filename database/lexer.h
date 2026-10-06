#pragma once
#include <core/nob.h>
#include <stdint.h>

#include "core/allocator.h"

typedef struct Scanner Scanner;

typedef struct Scanner {
    String_View sv;
    size_t start;
    size_t current;
    size_t line_start;
    int line;
    memory_arena* arena;
    size_t initialAlloc;
} Scanner;

typedef enum TokenType {
    TOKEN_EOF,
    TOKEN_LBRACKET,  // [
    TOKEN_RBRACKET,  // ]
    TOKEN_LBRACE,    // {
    TOKEN_RBRACE,    // }
    TOKEN_IDENTIFIER,
    TOKEN_TEXT,
    TOKEN_EQUAL,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_FLOAT,
    TOKEN_INTEGER,
    TOKEN_ERROR,
} TokenType;
const char* lx_tokenTypeToString(TokenType type);

typedef struct Token {
    TokenType type;
    union {
        String_View string;
        float f;
        int64_t i;
    } as;
    int line;
    int column;
} Token;

typedef struct arr_Tokens {
    Token* items;
    size_t capacity;
    size_t count;
} arr_Tokens;

void lx_init(Scanner* scanner, const char* source, memory_arena* _arena);
void lx_init_sv(Scanner* scanner, String_View sv, memory_arena* _arena);

arr_Tokens* lx_tokenize(Scanner* scanner);
const char* lx_tokenToStringArena(Token t, memory_arena* arena);
const char* lx_tokensToStringArena(arr_Tokens* tokens, memory_arena* arena);
