#include "node_pool.h"

#include "../../database/core/allocator.h"
#include "../../database/core/log.h"

u32 node_deref(NodePool* pool, NodeRef ref) {
    if (ref.idx > 0 && ref.idx < MAX_NODES && pool->used[ref.idx] &&
        pool->gen[ref.idx] == ref.gen) {
        return ref.idx;
    } else {
        return 0;
    }
}
NodeRef node_nil(void) {
    return (NodeRef){0, 0};
}
bool node_cmp(NodeRef n1, NodeRef n2) {
    if (n1.gen == n2.gen && n1.idx == n2.idx) {
        return true;
    } else {
        return false;
    }
}
void printPool(NodePool* pool) {
    String_Builder nextFreeSb = {0};
    String_Builder gen = {0};
    String_Builder used = {0};
    String_Builder idx = {0};

    sb_appendf(&nextFreeSb, "nextFree : [");
    sb_appendf(&gen, "gen      : [");
    sb_appendf(&used, "used     : [");
    sb_appendf(&idx, "idx      : [");

    for (u32 i = 0; i < MAX_NODES; i++) {
        sb_appendf(&nextFreeSb, "%2u ", pool->nextFree[i]);
        sb_appendf(&gen, "%2u ", pool->gen[i]);
        sb_appendf(&used, "%2u ", pool->used[i]);
        sb_appendf(&idx, "%2u ", i);
    }
    sb_appendf(&nextFreeSb, "]\n");
    sb_appendf(&gen, "]\n");
    sb_appendf(&used, "]\n");
    sb_appendf(&idx, "]\n");

    String_View sv = sb_to_sv(nextFreeSb);
    printf(SV_Fmt, SV_Arg(sv));
    sv = sb_to_sv(gen);
    printf(SV_Fmt, SV_Arg(sv));
    sv = sb_to_sv(used);
    printf(SV_Fmt, SV_Arg(sv));
    sv = sb_to_sv(idx);
    printf(SV_Fmt, SV_Arg(sv));

    printf("firstFree:%2u\n", pool->firstFree);
    printf("lastFree:%2u\n", pool->lastFree);

    printf("count:%2u\n", pool->count);
}
void node_pool_init(NodePool* pool) {
    memset(pool, 0, sizeof(*pool));
    for (uint32_t i = 1; i < MAX_NODES - 1; ++i) {
        pool->nextFree[i] = i + 1;
    }
    pool->nextFree[MAX_NODES - 1] = 0;
    pool->firstFree = 1;
    pool->lastFree = MAX_NODES - 1;
    pool->count = 0;
}
UaNodeIdExpanded* node_pool_get(NodePool* pool, NodeRef ref) {
    return &pool->items[node_deref(pool, ref)];
}
uint32_t node_pool_find_empty(NodePool* pool) {
    return pool->firstFree;
}
NodeRef node_pool_add(NodePool* pool, UaNodeIdExpanded node) {
    uint32_t slot = pool->firstFree;
    assert(slot != 0 && "Pool is full");

    log_trace("Adding node to slot:[%u]", slot);
    pool->firstFree = pool->nextFree[slot];
    if (pool->firstFree == 0) {
        pool->lastFree = 0;
    }

    pool->nextFree[slot] = 0;
    pool->items[slot] = node;
    pool->used[slot] = true;
    pool->gen[slot]++;
    pool->count++;

    return (NodeRef){slot, pool->gen[slot]};
}
void node_pool_remove(NodePool* pool, NodeRef ref) {
    u32 idx = node_deref(pool, ref);
    assert(idx != 0 && "Trying to remove nil");

    log_trace("Removing node from slot:[%u]", idx);
    pool->used[idx] = false;
    pool->nextFree[idx] = 0;

    if (pool->lastFree != 0) {
        pool->nextFree[pool->lastFree] = idx;
    } else {
        pool->firstFree = idx;
    }

    pool->lastFree = idx;
    pool->count--;
}
void node_pool_add_child(NodePool* nodes, NodeRef childRef, NodeRef targetRef) {
    UaNodeIdExpanded* parent = node_pool_get(nodes, targetRef);
    UaNodeIdExpanded* child = node_pool_get(nodes, childRef);
    // TODO: add some debug time error checking??

    if (node_cmp(parent->firstChildRef, NODE_NILL)) {
        parent->firstChildRef = childRef;
        child->parentRef = targetRef;
        child->nextSiblingRef = childRef;
        child->prevSiblingRef = childRef;
        parent->childrenCount++;
        child->childNumber = parent->childrenCount;
        return;
    }
    child->parentRef = targetRef;

    UaNodeIdExpanded* firstChild = node_pool_get(nodes, parent->firstChildRef);
    if (node_cmp(firstChild->prevSiblingRef, NODE_NILL)) {
        firstChild->prevSiblingRef = childRef;
        firstChild->nextSiblingRef = childRef;
        child->nextSiblingRef = parent->firstChildRef;
        child->prevSiblingRef = parent->firstChildRef;
    } else {
        NodeRef oldLastSiblingRef = firstChild->prevSiblingRef;
        firstChild->prevSiblingRef = childRef;
        child->nextSiblingRef = parent->firstChildRef;
        node_pool_get(nodes, oldLastSiblingRef)->nextSiblingRef = childRef;
        child->prevSiblingRef = oldLastSiblingRef;
    }

    parent->childrenCount++;
    child->childNumber = parent->childrenCount;
}
void UA_NodeId_copy_arena(memory_arena* strArena, const UA_NodeId* src, UA_NodeId* dst) {
    memset(dst, 0, sizeof(UA_NodeId));
    dst->namespaceIndex = src->namespaceIndex;
    dst->identifierType = src->identifierType;

    switch (src->identifierType) {
        case UA_NODEIDTYPE_NUMERIC: dst->identifier.numeric = src->identifier.numeric; break;
        case UA_NODEIDTYPE_GUID: dst->identifier.guid = src->identifier.guid; break;
        case UA_NODEIDTYPE_STRING: {
            size_t len = src->identifier.string.length;
            dst->identifier.string.length = len;
            if (len) {
                void* mem = arena_alloc(strArena, len, alignof(UA_Byte));
                memcpy(mem, src->identifier.string.data, len);
                dst->identifier.string.data = (UA_Byte*)mem;
            } else {
                dst->identifier.string.data = NULL;
            }
            break;
        }
        case UA_NODEIDTYPE_BYTESTRING: {
            size_t len = src->identifier.byteString.length;
            dst->identifier.byteString.length = len;
            if (len) {
                void* mem = arena_alloc(strArena, len, alignof(UA_Byte));
                memcpy(mem, src->identifier.byteString.data, len);
                dst->identifier.byteString.data = (UA_Byte*)mem;
            } else {
                dst->identifier.byteString.data = NULL;
            }
            break;
        }
        default: break;
    }
}
void UA_String_arena_copy(memory_arena* arena, const UA_String* src, UA_String* dst) {
    dst->length = src->length;
    if (dst->length) {
        void* mem = arena_alloc(arena, dst->length, alignof(UA_Byte));
        memcpy(mem, src->data, dst->length);
        dst->data = (UA_Byte*)mem;
    } else {
        dst->data = NULL;
    }
}

void UA_QualifiedName_arena_copy(
    memory_arena* strArena, const UA_QualifiedName* src, UA_QualifiedName* dst) {
    dst->namespaceIndex = src->namespaceIndex;
    UA_String_arena_copy(strArena, &src->name, &dst->name);
}
void UA_LocalizedText_arena_copy(
    memory_arena* strArena, const UA_LocalizedText* src, UA_LocalizedText* dst) {
    UA_String_arena_copy(strArena, &src->text, &dst->text);
    UA_String_arena_copy(strArena, &src->locale, &dst->locale);
}

void UA_NodeId_Expanded_arena_copy(
    memory_arena* strArena, const UA_ReferenceDescription* src, UaNodeIdExpanded* dst) {
    memset(dst, 0, sizeof(*dst));
    UA_NodeId_copy_arena(strArena, &src->nodeId.nodeId, &dst->nodeid);
    dst->nodeClass = src->nodeClass;
    UA_QualifiedName_arena_copy(strArena, &src->browseName, &dst->browseName);
    UA_LocalizedText_arena_copy(strArena, &src->displayName, &dst->displayName);
}
void UA_nodeId_toString(memory_arena* arena, String_Builder* sb, UA_NodeId nodeid) {
    switch (nodeid.identifierType) {
        case UA_NODEIDTYPE_NUMERIC:
            sb_appendf_arena(
                arena, sb, "ns=%u;i=%u", nodeid.namespaceIndex, nodeid.identifier.numeric);
            // offset += snprintf(
            //     buf, bufsize, "ns=%u;i=%u", namespaceIndex,
            //     identifier.numeric);
            break;
        case UA_NODEIDTYPE_STRING:
            sb_appendf_arena(
                arena,
                sb,
                "ns=%u;s=%.*s",
                nodeid.namespaceIndex,
                (int)nodeid.identifier.string.length,
                nodeid.identifier.string.data);

            // offset += snprintf(
            //     buf,
            //     bufsize,
            //     "ns=%u;s=%.*s",
            //     namespaceIndex,
            //     (int)identifier.string.length,
            //     identifier.string.data);
            break;
        case UA_NODEIDTYPE_GUID: {
            // const UA_Guid* g = &identifier.guid;
            // offset += snprintf(
            //     buf,
            //     bufsize,
            //     "ns=%u;g=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            //     namespaceIndex,
            //     g->data1,
            //     g->data2,
            //     g->data3,
            //     g->data4[0],
            //     g->data4[1],
            //     g->data4[2],
            //     g->data4[3],
            //     g->data4[4],
            //     g->data4[5],
            //     g->data4[6],
            //     g->data4[7]);
            break;
        }
    }
}
void UA_NodeId_Expanded_toString(
    memory_arena* arena, String_Builder* sb, const UaNodeIdExpanded* node) {
    UA_nodeId_toString(arena, sb, node->nodeid);
    sb_appendf_arena(arena, sb, "", (int)node->browseName.name.length, node->browseName.name.data);
}

void visit_print(
    memory_arena* arena, String_Builder* sb, NodePool* pool, NodeRef ref, u32 depth, void* user) {
    (void)user;
    UaNodeIdExpanded* node = node_pool_get(pool, ref);
    sb_appendf_arena(
        arena, sb, "%*s[%u][%u:%u] ", (int)(depth * 2), "", node->childNumber, ref.idx, ref.gen);
    UA_NodeId_Expanded_toString(arena, sb, node);
    sb_appendf_arena(arena, sb, "\n");
}
void visit_direct_children(
    memory_arena* arena,
    String_Builder* sb,
    NodePool* pool,
    NodeRef parentRef,
    NodeVisitor visitor,
    void* user) {
    UaNodeIdExpanded* parent = node_pool_get(pool, parentRef);
    NodeRef firstChildRef = parent->firstChildRef;

    if (node_cmp(firstChildRef, NODE_NILL)) {
        log_warn("No Children to print!");
        return;
    }

    // UaNodeIdExpanded* firstChild = node_pool_get(pool, parent->firstChildRef);
    NodeRef cur = firstChildRef;
    do {
        visitor(arena, sb, pool, cur, 1, user);
        UaNodeIdExpanded* n = node_pool_get(pool, cur);
        cur = n->nextSiblingRef;
    } while (!node_cmp(cur, firstChildRef));
}
void traverse_subtree_visit(
    memory_arena* arena,
    String_Builder* sb,
    NodePool pool,
    NodeRef idx,
    u32 depth,
    NodeVisitor visitor,
    void* user) {
    // if (idx == 0) return;

    // visitor(arena, sb, nodes, idx, depth, user);
    //
    // // child is deeper
    // traverse_subtree_visit(arena, sb, nodes, nodes[idx].firstChildIdx, depth + 1, visitor, user);
    //
    // // sibling stays same depth
    // traverse_subtree_visit(arena, sb, nodes, nodes[idx].nextSiblingIdx, depth, visitor, user);
}
