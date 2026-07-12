#pragma once
#include <stdbool.h>

#include "core/allocator.h"
#include "core/string.h"
#define TOSTRING(x) #x
typedef struct Scanner Scanner;

typedef struct Scanner {
    Sv sv;
    size_t start;
    size_t current;
    size_t line_start;
    int line;
    memory_arena* arena;
    size_t initialAlloc;
} Scanner;

typedef enum TokenType {
    TOKEN_EOF,
    TOKEN_LBRACKET,
    TOKEN_IDENTIFIER,
    TOKEN_RBRACKET,
    TOKEN_TEXT,
    TOKEN_EQUAL,
} TokenType;
const char* lx_tokenTypeToString(TokenType type);

typedef struct Token {
    TokenType type;
    Sv str;
    int line;
    int column;
} Token;

typedef struct arr_Tokens {
    Token* items;
    size_t capacity;
    size_t count;
} arr_Tokens;

void lx_init(Scanner* scanner, const char* source, memory_arena* _arena);
arr_Tokens* lx_tokenize(Scanner* scanner);
const char* lx_tokenToStringArena(Token t, memory_arena* arena);
const char* lx_tokensToStringArena(arr_Tokens* tokens, memory_arena* arena);
