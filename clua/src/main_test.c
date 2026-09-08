#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../database/core/core.h"
#include "../../database/core/types.h"
#include "log.h"
#include "mirua_types_internal.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/client_subscriptions.h"
#include "open62541/types.h"

static void UA_nodeId_toString(memory_arena* arena, String_Builder* sb, UA_NodeId nodeid) {
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
static void UA_NodeId_copy_arena(memory_arena* strArena, const UA_NodeId* src, UA_NodeId* dst) {
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
static void UA_String_arena_copy(memory_arena* arena, const UA_String* src, UA_String* dst) {
    dst->length = src->length;
    if (dst->length) {
        void* mem = arena_alloc(arena, dst->length, alignof(UA_Byte));
        memcpy(mem, src->data, dst->length);
        dst->data = (UA_Byte*)mem;
    } else {
        dst->data = NULL;
    }
}

static void UA_QualifiedName_arena_copy(
    memory_arena* strArena, const UA_QualifiedName* src, UA_QualifiedName* dst) {
    dst->namespaceIndex = src->namespaceIndex;
    UA_String_arena_copy(strArena, &src->name, &dst->name);
}
static void UA_LocalizedText_arena_copy(
    memory_arena* strArena, const UA_LocalizedText* src, UA_LocalizedText* dst) {
    UA_String_arena_copy(strArena, &src->text, &dst->text);
    UA_String_arena_copy(strArena, &src->locale, &dst->locale);
}

typedef struct {
    UA_NodeId nodeid;
    UA_QualifiedName browseName;
    UA_LocalizedText displayName;
    UA_NodeClass nodeClass;
    u32 parentIdx;
    u32 firstChildIdx;
    u32 nextSiblingIdx;
    u32 prevSiblingIdx;
} UA_NodeId_Expanded;

typedef struct {
    UA_NodeId_Expanded* items;
    size_t count;
    size_t capacity;
    // memory_arena nodeArena; //TODO: add these here instead of supplying them?
    // memory_arena stringArena;
} arr_NodeId_Expanded;

static void UA_NodeId_Expanded_arena_copy(
    memory_arena* strArena, const UA_ReferenceDescription* src, UA_NodeId_Expanded* dst) {
    memset(dst, 0, sizeof(*dst));
    UA_NodeId_copy_arena(strArena, &src->nodeId.nodeId, &dst->nodeid);
    dst->nodeClass = src->nodeClass;
    UA_QualifiedName_arena_copy(strArena, &src->browseName, &dst->browseName);
    UA_LocalizedText_arena_copy(strArena, &src->displayName, &dst->displayName);
}

typedef void (*NodeVisitor)(
    memory_arena* arena,
    String_Builder* sb,
    arr_NodeId_Expanded* nodes,
    u32 idx,
    u32 depth,
    void* user);

static void traverse_subtree_visit(
    memory_arena* arena,
    String_Builder* sb,
    arr_NodeId_Expanded* nodes,
    u32 idx,
    u32 depth,
    NodeVisitor visitor,
    void* user) {
    if (idx == 0) return;

    visitor(arena, sb, nodes, idx, depth, user);

    // child is deeper
    traverse_subtree_visit(
        arena, sb, nodes, nodes->items[idx].firstChildIdx, depth + 1, visitor, user);

    // sibling stays same depth
    traverse_subtree_visit(
        arena, sb, nodes, nodes->items[idx].nextSiblingIdx, depth, visitor, user);
}

static void UA_NodeId_Expanded_toString(
    memory_arena* arena, String_Builder* sb, UA_NodeId_Expanded* node) {
    UA_nodeId_toString(arena, sb, node->nodeid);
    sb_appendf_arena(
        arena,
        sb,
        "  -  [BrowseName: %.*s]",
        (int)node->browseName.name.length,
        node->browseName.name.data);
}

static void visit_print(
    memory_arena* arena,
    String_Builder* sb,
    arr_NodeId_Expanded* nodes,
    u32 idx,
    u32 depth,
    void* user) {
    (void)user;
    sb_appendf_arena(arena, sb, "%*s[%u] ", (int)(depth * 2), "", idx);
    UA_NodeId_Expanded_toString(arena, sb, &nodes->items[idx]);
    sb_appendf_arena(arena, sb, "\n");
}

static void visit_direct_children(
    memory_arena* arena,
    String_Builder* sb,
    arr_NodeId_Expanded* nodes,
    u32 parent_idx,
    NodeVisitor visitor,
    void* user) {
    if (parent_idx == 0) return;

    u32 child_idx = nodes->items[parent_idx].firstChildIdx;
    while (child_idx != 0) {
        visitor(arena, sb, nodes, child_idx, 1, user);
        child_idx = nodes->items[child_idx].nextSiblingIdx;
    }
}

static void explore_children(
    memory_arena* nodeArena,
    memory_arena* strArena,
    UA_Client* client,
    arr_NodeId_Expanded* nodes,
    u64 parent_idx) {
    UA_NodeId_Expanded* parent = &nodes->items[parent_idx];

    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse = UA_Array_new(1, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]);
    UA_BrowseDescription_init(&bReq.nodesToBrowse[0]);
    UA_NodeId_copy(&parent->nodeid, &bReq.nodesToBrowse[0].nodeId);
    bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bReq.nodesToBrowse[0].referenceTypeId = UA_NODEID_NUMERIC(
        0, UA_NS0ID_HIERARCHICALREFERENCES);  // Hierarchical references (includes HasComponent)
    bReq.nodesToBrowse[0].includeSubtypes = true;
    bReq.nodesToBrowse[0].nodeClassMask = 0;
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

    UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);

    // Process results
    if (bResp.resultsSize > 0) {
        u32 first_child = 0;
        u32 prev = 0;
        for (size_t i = 0; i < bResp.results[0].referencesSize; ++i) {
            UA_ReferenceDescription* ref = &bResp.results[0].references[i];

            UA_NodeId_Expanded child = {0};
            UA_NodeId_Expanded_arena_copy(strArena, ref, &child);
            child.parentIdx = (u32)parent_idx;
            child.prevSiblingIdx = prev;
            child.nextSiblingIdx = 0;
            child.firstChildIdx = 0;

            size_t child_idx = nodes->count;
            da_arena_append(nodeArena, nodes, child, UA_NodeId_Expanded);

            if (first_child == 0) first_child = (u32)child_idx;
            if (prev != 0) nodes->items[prev].nextSiblingIdx = (u32)child_idx;

            prev = (u32)child_idx;
        }
        parent->firstChildIdx = first_child;
    } else {
        log_warn("[EXPLORE] Browse failed or no results for node");
    }

    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);
}

// if (bResp.resultsSize > 0) {
//     u32 first_child = 0;
//     u32 prev = 0;
//     for (size_t i = 0; i < bResp.results[0].referencesSize; ++i) {
//         UA_ReferenceDescription* ref = &bResp.results[0].references[i];
//
//         UA_NodeId_Expanded child = {0};
//         UA_NodeId_Expanded_arena_copy(strArena, ref, &child);
//         child.parentIdx = (u32)parent_idx;
//         child.prevSiblingIdx = prev;
//         child.nextSiblingIdx = 0;
//         child.firstChildIdx = 0;
//
//         size_t child_idx = nodes->count;
//         da_arena_append(nodeArena, nodes, child, UA_NodeId_Expanded);
//
//         if (first_child == 0) first_child = (u32)child_idx;
//         if (prev != 0) nodes->items[prev].nextSiblingIdx = (u32)child_idx;
//
//         prev = (u32)child_idx;
//     }
//     parent->firstChildIdx = first_child;
// } else {
//     log_warn("[EXPLORE] Browse failed or no results for node");
// }

static void print_direct_children(
    memory_arena* arena, String_Builder* sb, arr_NodeId_Expanded* nodes, u32 parent_idx) {
    if (parent_idx == 0) return;

    u32 child_idx = nodes->items[parent_idx].firstChildIdx;
    while (child_idx != 0) {
        UA_NodeId_Expanded* c = &nodes->items[child_idx];
        sb_appendf_arena(arena, sb, "[%u] ", child_idx);
        UA_NodeId_Expanded_toString(arena, sb, c);
        sb_appendf_arena(arena, sb, "\n");
        child_idx = c->nextSiblingIdx;
    }
}

static void traverse_subtree(
    memory_arena* arena, String_Builder* sb, arr_NodeId_Expanded* nodes, u32 idx, u32 indent) {
    if (idx == 0) return;  // 0 = null/sentinel

    UA_NodeId_Expanded* n = &nodes->items[idx];

    sb_appendf_arena(arena, sb, "%*s[%u] ", (int)(indent * 2), "", idx);
    UA_NodeId_Expanded_toString(arena, sb, n);
    sb_appendf_arena(arena, sb, "\n");

    // child is deeper
    traverse_subtree(arena, sb, nodes, n->firstChildIdx, indent + 1);

    // sibling stays same depth
    traverse_subtree(arena, sb, nodes, n->nextSiblingIdx, indent);
}

struct Timer {
    double start;
    size_t count;
};
// exmaple use
// String_Builder sb = {0};
// arena_reset(printArena, false);
// struct Timer t1 = {0};
// timer_start(&t1);
// visit_direct_children(printArena, &sb, &nodes, 1, visit_print, NULL);
// timer_mark(&t1);
// printf(
//     "callback: %.6f s total, %.6f us each\n",
//     timer_elapsed_sec(&t1),
//     1e6 * timer_elapsed_sec(&t1) / (double)t1.count);
// String_View sv = sb_to_sv(sb);
// printf(SV_Fmt "\n", SV_ARG(sv));

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void timer_start(struct Timer* t) {
    t->count = 0;
    t->start = now_sec();
}

static void timer_mark(struct Timer* t) {
    t->count++;
}

static double timer_elapsed_sec(struct Timer* t) {
    return now_sec() - t->start;
}

int main(int argc, char* argv[]) {
    UA_Client* client = UA_Client_new();
    UA_ClientConfig* cc = UA_Client_getConfig(client);
    UA_ClientConfig_setDefault(cc);
    const char* uaEndpoint = "opc.tcp://localhost:4840";
    UA_StatusCode retval = UA_Client_connect(client, uaEndpoint);

    // UA_NodeId root = UA_NODEID_NUMERIC(0, 85);
    memory_arena* nodeArena = arena_create(MB(2));
    memory_arena* strArena = arena_create(MB(2));

    arr_NodeId_Expanded nodes = {0};
    // TODO: intialize proper sentinel.
    da_arena_append(nodeArena, &nodes, (UA_NodeId_Expanded){0}, UA_NodeId_Expanded);

    size_t root_idx = nodes.count;
    da_arena_append(
        nodeArena,
        &nodes,
        (UA_NodeId_Expanded){.nodeid = UA_NODEID_NUMERIC(0, 85)},
        UA_NodeId_Expanded);

    explore_children(nodeArena, strArena, client, &nodes, root_idx);

    memory_arena* printArena = arena_create(MB(1));
    // sb_arena_append_cstr(printArena, &sb, "hello");
    // String_Builder sb = {0};
    // sb.count = 0;
    // for (size_t i = 0; i < nodes.count; i++) {
    //     UA_NodeId_Expanded curNode = nodes.items[i];
    //     sb_appendf_arena(printArena, &sb, "[%zu]", i);
    //     UA_NodeId_Expanded_toString(printArena, &sb, &curNode);
    //     sb_appendf_arena(printArena, &sb, "\n");
    // }
    // String_View sv = sb_to_sv(sb);
    // printf(SV_Fmt "\n", SV_ARG(sv));

    String_Builder sb = {0};
    arena_reset(printArena, false);
    struct Timer t1 = {0};
    timer_start(&t1);
    visit_direct_children(printArena, &sb, &nodes, 1, visit_print, NULL);
    timer_mark(&t1);
    printf(
        "callback: %.6f s total, %.6f us each\n",
        timer_elapsed_sec(&t1),
        1e6 * timer_elapsed_sec(&t1) / (double)t1.count);
    String_View sv = sb_to_sv(sb);
    printf(SV_Fmt "\n", SV_ARG(sv));

    return 0;
}
