#include <assert.h>
#include <replxx.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
    bool explored;
} UaNodeIdExpanded;

#define MAX_NODES 5
// firstFree  = 2
// lastFree = 7
// nextfree : [0,0,3,7,0,0,0,0,0,0]
// items :    [x,x,d,d,x,x,x,d,x,x]
// idx :      [0,1,2,3,4,5,6,7,8,9]
//

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

typedef struct NodeRef {
    u32 idx;
    u32 gen;
} NodeRef;
static u32 node_deref(NodePool* pool, NodeRef ref) {
    if (ref.idx > 0 && ref.idx < MAX_NODES && pool->used[ref.idx] &&
        pool->gen[ref.idx] == ref.gen) {
        return ref.idx;
    } else {
        return 0;
    }
}
static NodeRef node_nil(void) {
    return (NodeRef){0, 0};
}

static void printPool(NodePool* pool) {
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
    printf(SV_Fmt, SV_ARG(sv));
    sv = sb_to_sv(gen);
    printf(SV_Fmt, SV_ARG(sv));
    sv = sb_to_sv(used);
    printf(SV_Fmt, SV_ARG(sv));
    sv = sb_to_sv(idx);
    printf(SV_Fmt, SV_ARG(sv));

    printf("firstFree:%2u\n", pool->firstFree);
    printf("lastFree:%2u\n", pool->lastFree);

    printf("count:%2u\n", pool->count);
}

static void node_pool_init(NodePool* pool) {
    memset(pool, 0, sizeof(*pool));
    for (uint32_t i = 1; i < MAX_NODES - 1; ++i) {
        pool->nextFree[i] = i + 1;
    }
    pool->nextFree[MAX_NODES - 1] = 0;
    pool->firstFree = 1;
    pool->lastFree = MAX_NODES - 1;
    pool->count = 0;
}

static UaNodeIdExpanded* node_pool_get(NodePool* pool, NodeRef ref) {
    return &pool->items[node_deref(pool, ref)];
}

static uint32_t node_pool_find_empty(NodePool* pool) {
    return pool->firstFree;
}

static NodeRef NodePool_add(NodePool* pool, UaNodeIdExpanded node) {
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

static void node_pool_remove(NodePool* pool, NodeRef ref) {
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

static void NodePool_add_child(NodePool* nodes, u32 childIdx, u32 targetIdx) {
    UaNodeIdExpanded* target = &nodes->items[targetIdx];
    UaNodeIdExpanded* child = &nodes->items[childIdx];

    if (target->firstChildIdx == 0) {
        target->firstChildIdx = childIdx;
        child->parentIdx = targetIdx;
        return;
    }
    child->parentIdx = targetIdx;

    UaNodeIdExpanded* firstChild = &nodes->items[target->firstChildIdx];
    // If firstchild doesnt have previous. it is the last
    if (firstChild->prevSiblingIdx == 0) {
        firstChild->prevSiblingIdx = childIdx;
        firstChild->nextSiblingIdx = childIdx;
        child->nextSiblingIdx = target->firstChildIdx;
        child->prevSiblingIdx = target->firstChildIdx;
    } else {
        u32 oldLastSiblingIdx = firstChild->prevSiblingIdx;
        firstChild->prevSiblingIdx = childIdx;
        child->nextSiblingIdx = target->firstChildIdx;
        nodes->items[oldLastSiblingIdx].nextSiblingIdx = childIdx;
        child->prevSiblingIdx = oldLastSiblingIdx;
    }
}

typedef struct {
    UaNodeIdExpanded* items;
    size_t count;
    size_t capacity;
    // memory_arena nodeArena; //TODO: add these here instead of supplying them?
    // memory_arena stringArena;
} arr_NodeId_Expanded;

static void UA_NodeId_Expanded_arena_copy(
    memory_arena* strArena, const UA_ReferenceDescription* src, UaNodeIdExpanded* dst) {
    memset(dst, 0, sizeof(*dst));
    UA_NodeId_copy_arena(strArena, &src->nodeId.nodeId, &dst->nodeid);
    dst->nodeClass = src->nodeClass;
    UA_QualifiedName_arena_copy(strArena, &src->browseName, &dst->browseName);
    UA_LocalizedText_arena_copy(strArena, &src->displayName, &dst->displayName);
}

typedef void (*NodeVisitor)(
    memory_arena* arena,
    String_Builder* sb,
    UaNodeIdExpanded nodes[],
    u32 idx,
    u32 depth,
    void* user);

static void traverse_subtree_visit(
    memory_arena* arena,
    String_Builder* sb,
    UaNodeIdExpanded nodes[],
    u32 idx,
    u32 depth,
    NodeVisitor visitor,
    void* user) {
    if (idx == 0) return;

    visitor(arena, sb, nodes, idx, depth, user);

    // child is deeper
    traverse_subtree_visit(arena, sb, nodes, nodes[idx].firstChildIdx, depth + 1, visitor, user);

    // sibling stays same depth
    traverse_subtree_visit(arena, sb, nodes, nodes[idx].nextSiblingIdx, depth, visitor, user);
}

static void UA_NodeId_Expanded_toString(
    memory_arena* arena, String_Builder* sb, const UaNodeIdExpanded* node) {
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
    UaNodeIdExpanded nodes[],
    u32 idx,
    u32 depth,
    void* user) {
    (void)user;
    UaNodeIdExpanded* node = &nodes[idx];
    sb_appendf_arena(arena, sb, "%*s[%u] ", (int)(depth * 2), "", idx);
    UA_NodeId_Expanded_toString(arena, sb, node);
    sb_appendf_arena(arena, sb, "\n");
}

static void visit_direct_children(
    memory_arena* arena,
    String_Builder* sb,
    UaNodeIdExpanded nodes[],
    u32 parent_idx,
    NodeVisitor visitor,
    void* user) {
    if (parent_idx == 0) return;

    u32 child_idx = nodes[parent_idx].firstChildIdx;
    if (child_idx == 0) {
        log_warn("No Children!");
        return;
    }

    u32 firstChild = child_idx;
    do {
        visitor(arena, sb, nodes, child_idx, 1, user);
        child_idx = nodes[child_idx].nextSiblingIdx;

    } while (child_idx != firstChild);
}

static void explore_children(
    memory_arena* strArena, UA_Client* client, NodePool* nodes, u64 targetIdx) {
    UaNodeIdExpanded* root = &nodes->items[targetIdx];

    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse = UA_Array_new(1, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]);
    UA_BrowseDescription_init(&bReq.nodesToBrowse[0]);
    UA_NodeId_copy(&root->nodeid, &bReq.nodesToBrowse[0].nodeId);
    bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bReq.nodesToBrowse[0].referenceTypeId = UA_NODEID_NUMERIC(
        0, UA_NS0ID_HIERARCHICALREFERENCES);  // Hierarchical references (includes HasComponent)
    bReq.nodesToBrowse[0].includeSubtypes = true;
    bReq.nodesToBrowse[0].nodeClassMask = 0;
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

    UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);

    if (bResp.resultsSize > 0) {
        for (size_t i = 0; i < bResp.results[0].referencesSize; ++i) {
            UA_ReferenceDescription* ref = &bResp.results[0].references[i];
            UaNodeIdExpanded child = {0};

            UA_NodeId_Expanded_arena_copy(strArena, ref, &child);
            // u32 childIdx = NodePool_add(nodes, child);
            // NodePool_add_child(nodes, childIdx, targetIdx);
        }
    } else {
        log_warn("[EXPLORE] Browse failed or no results for node");
    }

    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);
}

static void traverse_subtree(
    memory_arena* arena, String_Builder* sb, arr_NodeId_Expanded* nodes, u32 idx, u32 indent) {
    if (idx == 0) return;  // 0 = null/sentinel

    UaNodeIdExpanded* n = &nodes->items[idx];

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
// Command structure
typedef struct {
    const char* name;
    const char* description;
    uint32_t state;
    void (*handler)(void* ctx, const char* args);
} Command;
void cmd_ls(void* userdata, const char* args);
void cmd_cd(void* userdata, const char* args);
void cmd_browse(void* userdata, const char* args);
void cmd_cd_up(void* userdata, const char* args);

static const Command commands[] = {
    // Main context commands
    {"ls", "Connect to OPC-UA server", 0, cmd_ls},
    {"cd", "Connect to OPC-UA server", 0, cmd_cd},
    {"cd..", "Connect to OPC-UA server", 0, cmd_cd_up},
    {"browse", "Connect to OPC-UA server", 0, cmd_browse},

};
#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

void dispatch_command(const char* input, uint32_t state, void* userdata) {
    char cmd[64], arg[256];
    int args_parsed = sscanf(input, "%63s %255[^\n]", cmd, arg);

    if (args_parsed < 1) return;

    // Find and execute command
    for (size_t i = 0; i < COMMAND_COUNT; ++i) {
        if (commands[i].state == 0) {
            if (strcmp(cmd, commands[i].name) == 0) {
                const char* args_str = (args_parsed >= 2) ? arg : "";
                commands[i].handler(userdata, args_str);
                return;
            }
        }
    }

    printf("Unknown command: %s\n", cmd);
}

typedef struct context {
    memory_arena* strArena;
    memory_arena* printArena;
    NodePool nodes;
    UA_Client* client;
    uint32_t current;
    uint32_t root;
} context;

static UaNodeIdExpanded* thing_get_current_node(context* ctx) {
    assert(ctx->current != 0 && "sholdnt happen lol");
    return &ctx->nodes.items[ctx->current];
}

void cmd_ls(void* userdata, const char* args) {
    context* ctx = (context*)userdata;
    String_Builder sb = {0};
    arena_reset(ctx->printArena, false);
    sb_appendf_arena(ctx->printArena, &sb, "%*s{%u} ", (int)(0 * 2), "", ctx->current);
    UA_NodeId_Expanded_toString(ctx->printArena, &sb, thing_get_current_node(ctx));
    sb_appendf_arena(ctx->printArena, &sb, "\n");

    visit_direct_children(ctx->printArena, &sb, ctx->nodes.items, ctx->current, visit_print, NULL);
    String_View sv = sb_to_sv(sb);
    printf(SV_Fmt "\n", SV_ARG(sv));
}
void cmd_cd_up(void* userdata, const char* args) {
    context* ctx = (context*)userdata;
    String_Builder sb = {0};
    arena_reset(ctx->printArena, false);

    // UA_NodeId_Expanded* currentNode = thing_get_current_node(ctx);
    // u32 parentIdx = currentNode->parentIdx;
    // if (!parentIdx) {
    //     log_warn("No parent to cd.. back to");
    //     return;
    // }
    //
    // ctx->current = parentIdx;
    // if (currentNode->explored) {
    //     visit_direct_children(
    //         ctx->printArena, &sb, ctx->nodes.items, ctx->current, visit_print, NULL);
    //     String_View sv = sb_to_sv(sb);
    //     printf(SV_Fmt "\n", SV_ARG(sv));
    // } else {
    //     ctx->nodes.items[ctx->current].explored = true;
    //     explore_children(
    //         ctx->nodeArena, ctx->strArena, ctx->client, ctx->nodes.items, ctx->current);
    //     visit_direct_children(
    //         ctx->printArena, &sb, ctx->nodes.items, ctx->current, visit_print, NULL);
    //     String_View sv = sb_to_sv(sb);
    //     printf(SV_Fmt "\n", SV_ARG(sv));
    // }
}

void cmd_cd(void* userdata, const char* args) {
    context* ctx = (context*)userdata;
    memory_arena* printArena = arena_create(MB(3));
    String_Builder sb = {0};
    arena_reset(printArena, false);

    // if (strcmp(args, "..") == 0) {
    //     cmd_cd_up(ctx, "");
    // }
    //
    // // TARGET NODE
    // int index;
    // if (sscanf(args, "%d", &index) != 1) {
    //     log_warn("No such node available");
    //     return;
    // }
    // ctx->current = index;
    // UA_NodeId_Expanded* node = &ctx->nodes.items[ctx->current];
    //
    // if (node->explored) {
    //     visit_direct_children(printArena, &sb, ctx->nodes.items, index, visit_print, NULL);
    //     String_View sv = sb_to_sv(sb);
    //     printf(SV_Fmt "\n", SV_ARG(sv));
    // } else {
    //     ctx->nodes.items[ctx->current].explored = true;
    //     explore_children(ctx->nodeArena, ctx->strArena, ctx->client, ctx->nodes.items, index);
    //     visit_direct_children(printArena, &sb, ctx->nodes.items, index, visit_print, NULL);
    //     String_View sv = sb_to_sv(sb);
    //     printf(SV_Fmt "\n", SV_ARG(sv));
    // }
}
void cmd_browse(void* userdata, const char* args) {
    // Should first delete the nodes children its browsing.
    // then rebrowse
    //     context* ctx = (context*)userdata;
    //     explore_children(ctx->nodeArena, ctx->strArena, ctx->client, &ctx->nodes,
    //     ctx->nodes.current); memory_arena* printArena = arena_create(MB(3));
    //
    //     String_Builder sb = {0};
    //     arena_reset(printArena, false);
    //     visit_direct_children(printArena, &sb, &ctx->nodes, 1, visit_print, NULL);
    //     String_View sv = sb_to_sv(sb);
    //     printf(SV_Fmt "\n", SV_ARG(sv));
}

void completion_callback(
    const char* prefix, replxx_completions* completions, int* context_len, void* user_data) {
    if (!prefix) {
        if (context_len) *context_len = 0;
        return;
    }

    // Find the start of the current word (after last space)
    const char* word_start = strrchr(prefix, ' ');
    if (word_start) {
        word_start++;  // Skip the space
    } else {
        word_start = prefix;  // No space found, entire prefix is the word
    }

    size_t word_len = strlen(word_start);
    if (context_len) *context_len = (int)word_len;

    // Only complete if we're at the beginning (first word = command)
    if (word_start == prefix) {
        for (size_t i = 0; i < COMMAND_COUNT; ++i) {
            if (commands[i].state == 0) {
                if (strncmp(commands[i].name, word_start, word_len) == 0) {
                    replxx_add_completion(completions, commands[i].name);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    printf("sizeof uanode:%zu", sizeof(UaNodeIdExpanded));
    UA_Client* client = UA_Client_new();
    UA_ClientConfig* cc = UA_Client_getConfig(client);
    UA_ClientConfig_setDefault(cc);
    const char* uaEndpoint = "opc.tcp://localhost:4840";
    UA_StatusCode retval = UA_Client_connect(client, uaEndpoint);

    // UA_NodeId root = UA_NODEID_NUMERIC(0, 85);
    context ctx;
    ctx.strArena = arena_create(MB(2));
    ctx.printArena = arena_create(MB(3));

    ctx.client = client;

    // arr_NodeId_Expanded nodes = {0};
    // memset(&ctx.nodes, 0, sizeof(ctx.nodes));
    node_pool_init(&ctx.nodes);

    log_set_level(LOG_TRACE);
    // ctx.root =
    NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 85)});
    ctx.current = ctx.root;
    printPool(&ctx.nodes);
    NodeRef ref1 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 15)});
    NodeRef ref2 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 25)});
    node_pool_remove(&ctx.nodes, ref1);
    node_pool_remove(&ctx.nodes, ref2);
    ref1 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 15)});
    ref2 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 25)});
    NodeRef ref3 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    printPool(&ctx.nodes);
    node_pool_remove(&ctx.nodes, ref3);
    ref1 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 15)});
    ref2 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 25)});
    ref3 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    node_pool_remove(&ctx.nodes, ref1);
    node_pool_remove(&ctx.nodes, ref2);
    node_pool_remove(&ctx.nodes, ref3);
    ref3 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    node_pool_remove(&ctx.nodes, ref3);
    ref3 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    node_pool_remove(&ctx.nodes, ref3);
    ref3 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    node_pool_remove(&ctx.nodes, ref3);
    ref3 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    node_pool_remove(&ctx.nodes, ref3);
    ref3 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});

    printPool(&ctx.nodes);

    // u32 idx4 = NodePool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 45)});

    String_Builder sb = {0};
    thing_get_current_node(&ctx)->explored = true;
    explore_children(ctx.strArena, ctx.client, &ctx.nodes, 1);
    visit_direct_children(ctx.printArena, &sb, ctx.nodes.items, 1, visit_print, NULL);
    String_View sv = sb_to_sv(sb);
    printf(SV_Fmt "\n", SV_ARG(sv));
    //
    // memory_arena* printArena = arena_create(MB(3));
    //
    // String_Builder sb = {0};
    // arena_reset(printArena, false);
    // visit_direct_children(printArena, &sb, &nodes, 1, visit_print, NULL);
    // String_View sv = sb_to_sv(sb);
    // printf(SV_Fmt "\n", SV_ARG(sv));

    Replxx* replxx = replxx_init();

    // Set autocomplete callback
    replxx_set_completion_callback(replxx, completion_callback, NULL);

    while (1) {
        const char* input = replxx_input(replxx, "mirwiz> ");
        if (!input) {
            printf("\nGoodbye!\n");
            break;
        }

        if (strlen(input) == 0) continue;

        replxx_history_add(replxx, input);

        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            printf("Goodbye!\n");
            break;
        }

        dispatch_command(input, 0, (void*)&ctx);
    }

    replxx_end(replxx);
    // mirua_module_free(ctx);

    return 0;
}
