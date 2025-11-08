#pragma once
#include <stdbool.h>

#include "mirua_types.h"
// Forward declaration for AST and ASTNode
typedef struct AST AST;
typedef struct ASTNode ASTNode;

typedef struct HelpContext HelpContext;
typedef struct FileContext FileContext;
typedef struct MiruaContext MiruaContext;

typedef struct {
    // Module contexts
    HelpContext* help_context;
    FileContext* file_context;
    MiruaContext* mirua_context;
} Interpreter;

// AST-based interpreter functions
Interpreter* interpreter_create_ast(void);
void interpreter_free_ast(Interpreter* interpreter);
void interpreter_execute_ast(Interpreter* interpreter, AST* ast);
MiruaState interpreter_get_mirua_state(Interpreter* interpreter);

// Actual meat of the stuff
void interpreter_normal_state_execute(Interpreter* interpreter, ASTNode* node);
void interpreter_config_state_execute(Interpreter* interpreter, ASTNode* node);
