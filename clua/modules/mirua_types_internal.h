#pragma once

#include <open62541/types.h>

typedef enum {
    MIRUA_NODE_STRUCTURE,  // Has childrenExtensionObject/complex type
    MIRUA_NODE_ARRAY,
    MIRUA_NODE_VALUE  // Leaf node primitive type like int, string
} MiruaNodeType;

typedef struct MiruaNodeId {
    UA_NodeId nodeid;
    UA_QualifiedName name;
} MiruaNodeId;

typedef struct MiruaValue {
    UA_Variant value;
    MiruaNodeId type;
} MiruaValue;

typedef struct MiruaTreeNode {
    MiruaNodeId nodeid;
    MiruaValue value;
    UA_NodeId type;
    MiruaNodeType nodeType;
    struct MiruaTreeNode** children;
    size_t child_count;
    size_t child_capacity;
} MiruaTreeNode;

typedef struct MiruaTree {
    MiruaTreeNode* root;
    size_t total_nodes;
} MiruaTree;

typedef struct {
    MiruaNodeId* nodeIds;
    size_t size;
    size_t capacity;  // TODO: learn how to implement dynamic size
} mirua_t_NodeList;

typedef struct {
    UA_NodeId* items;
    size_t count;
    size_t capacity;  // TODO: learn how to implement dynamic size
} arr_NodeId;

#define MAX_HISTORY 50
typedef struct {
    MiruaNodeId nodeIds[MAX_HISTORY];
    size_t count;
} NodeIdHistory;
