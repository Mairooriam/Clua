#pragma once
// Provided partly by claude
#define LEXER_DEBUG_PRINT  // TODO: Make proper debug logger

#include <stdbool.h>
#include <stdint.h>
typedef enum {
    // Parameter tokens
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_MINUS,
    TOKEN_DOT,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_ASSING,
    TOKEN_SLASH,
    TOKEN_PARAMETER_COUNT,

    // Command tokens
    TOKEN_HELP = TOKEN_PARAMETER_COUNT,
    TOKEN_BROWSE,
    TOKEN_MODIFIER,
    TOKEN_SAVE,
    TOKEN_ADD,
    TOKEN_LIST_DIRS,
    TOKEN_UNDO,
    TOKEN_REDO,
    TOKEN_CONNECT,
    TOKEN_DISCONNECT,
    TOKEN_CURRENT,
    TOKEN_SET,
    TOKEN_HISTORY,
    TOKEN_STATE,
    TOKEN_NAVIGATE,
    TOKEN_COMMAND_COUNT,

    // Other tokens
    TOKEN_EOF = TOKEN_COMMAND_COUNT,
    TOKEN_UNKNOWN,
    TOKEN_COUNT
} MirTokenType;

typedef struct {
    MirTokenType type;
    char *value;
    int line;
    int column;
} Token;

typedef struct {
    const char *source;
    int current;
    int line;
    int column;
    int length;
} Lexer;

// Main functions
Lexer *lexer_create(const char *source);
char lexer_peek(Lexer *lexer);
char lexer_advance(Lexer *lexer);
void lexer_skip_whitespace(Lexer *lexer);
Token *token_create(MirTokenType type, const char *value, int line, int column);
Token *lexer_next_token(Lexer *lexer);
Token **lexer_tokenize_all(const char *input, int *token_count);
void lexer_free_tokens(Token **tokens, int count);

// Clean up
void token_free(Token *token);
void lexer_free(Lexer *lexer);

// Helpers
void token_print(Token *token);
const char *lexer_to_string_token(const Token *token);
const char *lexer_to_string_tokentype(MirTokenType tokentype);
bool lexer_is_command_token(MirTokenType type);
bool lexer_is_parameter_token(MirTokenType type);
