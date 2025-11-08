#pragma once
#include <stdint.h>
typedef enum {
    AST_CONNECT,
    AST_LIST_DIRS,
    AST_DISCONNECT,
    AST_PARAMETER,
    AST_RANGE,
    AST_IP,
    AST_NODEID,
    AST_NAVIGATE_UP,  // TODO: ADD _NODE to enums
    AST_NAVIGATE_DOWN,
    AST_NAVIGATE_TO,
    AST_NODE_SAVE,
    AST_NODE_SET,
    AST_NODE_STATE,
} ASTNodeType;
#define AST_INITIAL_CAPACITY 8
#define AST_GROW_FACTOR 2
typedef struct {
    int start;
    int end;
} ASTRangeData;

typedef enum { AST_PARAM_TYPE_INT, AST_PARAM_TYPE_STRING } ASTParameterType;

typedef struct {
    ASTParameterType type;
    union {
        int i;
        char* s;
    } value;
} ASTParameterData;

typedef struct {
    int idx;
} ASTListDirsData;

typedef enum { NODEID_IDENTIFIER_STRING, NODEID_IDENTIFIER_INT } NodeIdIdentifierType;

typedef union {
    const char* s;
    int i;
} NodeIdIdentifier;

typedef struct {
    int ns;
    NodeIdIdentifierType type;
    NodeIdIdentifier identifier;
    const char* str;
} ASTNodeIdData;

typedef union {
    ASTRangeData range;
    ASTParameterData parameter;
    ASTNodeIdData nodeid;
    ASTListDirsData list_dirs;
} ASTNodeData;

typedef struct ASTNode {
    ASTNodeType type;
    ASTNodeData data;
    struct ASTNode** children;
    size_t child_count;
    size_t child_capacity;
} ASTNode;

typedef struct AST {
    ASTNode** nodes;
    size_t node_count;
    size_t node_capacity;
} AST;

// AST functions
AST* ast_create(void);
void ast_free(AST* ast);
void ast_node_free(ASTNode* node);

// AST types
ASTNode* ast_create_node(ASTNodeType type, ASTNodeData data);
ASTNode* ast_create_node_parameter(ASTParameterData data);
ASTNode* ast_create_node_range(int start, int end);
ASTNode* ast_create_node_ip(const char* ip_str);
ASTNode* ast_create_node_navigate_up(void);
ASTNode* ast_create_node_navigate_down(void);
ASTNode* ast_create_node_navigate_to(void);
ASTNode* ast_create_node_connect(void);
ASTNode* ast_create_node_list_dirs(ASTListDirsData data);
ASTNode* ast_create_node_nodeId(ASTNodeIdData data);
ASTNode* ast_create_node_save(void);
ASTNode* ast_create_node_state(void);
ASTNode* ast_create_node_set(void);
void ast_add_child(ASTNode* parent, ASTNode* child);
void ast_add_statement(AST* ast, ASTNode* statement);

// debug printing
void ast_to_string_ast(char* buf, size_t bufsize, size_t* offset, AST* ast);
void ast_to_string_node(char* buf, size_t bufsize, size_t* offset, ASTNode* node, int indent_level);
void ast_print_debug_ast(AST* ast);
void ast_print_debug_node(ASTNode* node);
const char* ast_to_string_nodetype(ASTNodeType type);
