#pragma once
#include "../../database/core/types.h"
#include "open62541/types.h"

typedef struct NodeRef {
    u32 idx;
    u32 gen;
} NodeRef;

typedef struct arr_NodeRef {
    NodeRef* items;
    size_t count;
    size_t capacity;
} arr_NodeRef;

#define NODE_NILL (NodeRef){0, 0}
NodeRef node_nil(void);

typedef struct {
    UA_NodeId nodeid;
    UA_QualifiedName browseName;
    UA_LocalizedText displayName;
    UA_NodeClass nodeClass;
    NodeRef parentRef;
    NodeRef firstChildRef;
    NodeRef nextSiblingRef;
    NodeRef prevSiblingRef;
    uint32_t childNumber;
    uint32_t childrenCount;
    bool explored;
} UaNodeIdExpanded;

typedef struct {
    UaNodeIdExpanded* items;
    size_t count;
    size_t capacity;
} arr_NodeId_Expanded;

#define MAX_NODES 150

typedef struct Node_Pool {
    UaNodeIdExpanded items[MAX_NODES];
    bool used[MAX_NODES];
    uint32_t nextFree[MAX_NODES];
    uint32_t gen[MAX_NODES];
    uint32_t firstFree;
    uint32_t lastFree;
    // TODO: do lastFree for reasons of first in last out
    uint32_t count;
} NodePool;

u32 node_deref(NodePool* pool, NodeRef ref);
bool node_cmp(NodeRef n1, NodeRef n2);

void node_pool_print(NodePool* pool);
void node_pool_init(NodePool* pool);
UaNodeIdExpanded* node_pool_get(NodePool* pool, NodeRef ref);
uint32_t node_pool_find_empty(NodePool* pool);
NodeRef node_pool_add(NodePool* pool, UaNodeIdExpanded node);
void node_pool_remove(NodePool* pool, NodeRef ref);
void node_pool_add_child(NodePool* nodes, NodeRef childRef, NodeRef targetRef);

void UA_NodeId_copy_arena(memory_arena* strArena, const UA_NodeId* src, UA_NodeId* dst);
void UA_String_arena_copy(memory_arena* arena, const UA_String* src, UA_String* dst);
void UA_QualifiedName_arena_copy(
    memory_arena* strArena, const UA_QualifiedName* src, UA_QualifiedName* dst);
void UA_LocalizedText_arena_copy(
    memory_arena* strArena, const UA_LocalizedText* src, UA_LocalizedText* dst);
void UA_NodeId_Expanded_arena_copy(
    memory_arena* strArena, const UA_ReferenceDescription* src, UaNodeIdExpanded* dst);
void UA_nodeId_toString(memory_arena* arena, String_Builder* sb, UA_NodeId nodeid);
void UA_NodeId_Expanded_toString(
    memory_arena* arena, String_Builder* sb, const UaNodeIdExpanded* node);

typedef void (*NodeVisitor)(
    memory_arena* arena, String_Builder* sb, NodePool* nodes, NodeRef ref, u32 depth, void* user);

typedef enum UserDataType {
    UDT_IDX,
} UserDataType;
typedef struct VisitorUserData {
    UserDataType type;
    union {
        uint32_t idx;
    };
} VisitorUserData;

void visit_print(
    memory_arena* arena, String_Builder* sb, NodePool* pool, NodeRef ref, u32 depth, void* user);
void visit_direct_children(
    memory_arena* arena,
    String_Builder* sb,
    NodePool* pool,
    NodeRef parentRef,
    NodeVisitor visitor,
    void* user);
void traverse_subtree_visit(
    memory_arena* arena,
    String_Builder* sb,
    NodePool pool,
    NodeRef idx,
    u32 depth,
    NodeVisitor visitor,
    void* user);

// expriment with this.
//    NodeChildIter it = node_children_begin(&ctx->nodes, ctx->current);
// for (;;) {
//     UaNodeIdExpanded* child = node_children_next(&it);
//     if (!child) break;

typedef struct {
    NodePool* pool;
    NodeRef first;
    NodeRef current;
    bool started;
} NodeChildIter;

static inline NodeChildIter node_children_begin(NodePool* pool, NodeRef parent) {
    UaNodeIdExpanded* p = node_pool_get(pool, parent);
    return (NodeChildIter){.pool = pool, .first = p->firstChildRef};
}

static inline UaNodeIdExpanded* node_children_next(NodeChildIter* it) {
    if (node_cmp(it->first, NODE_NILL)) return NULL;

    if (!it->started) {
        it->current = it->first;
        it->started = true;
        return node_pool_get(it->pool, it->current);
    }

    UaNodeIdExpanded* cur = node_pool_get(it->pool, it->current);
    it->current = cur->nextSiblingRef;
    if (node_cmp(it->current, it->first)) return NULL;
    return node_pool_get(it->pool, it->current);
}
