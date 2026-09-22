#pragma once
#include <replxx.h>

#include "../../database/core/types.h"
#include "node_pool.h"
#include "open62541/client.h"

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
void cmd_save(void* userdata, const char* args);

static const Command commands[] = {
    // Main context commands
    {"ls", "Connect to OPC-UA server", 0, cmd_ls},
    {"cd", "Connect to OPC-UA server", 0, cmd_cd},
    {"cd..", "Connect to OPC-UA server", 0, cmd_cd_up},
    {"browse", "Connect to OPC-UA server", 0, cmd_browse},
    {"save", "Connect to OPC-UA server", 0, cmd_save},

};
#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

void dispatch_command(const char* input, uint32_t state, void* userdata);
void completion_callback(
    const char* prefix, replxx_completions* completions, int* context_len, void* user_data);

typedef struct context {
    memory_arena* strArena;
    memory_arena* tempArena;
    NodePool nodes;
    UA_Client* client;
    NodeRef current;
    NodeRef root;
} context;

// TODO: move this elsewhere?
void explore_children(
    memory_arena* strArena, UA_Client* client, NodePool* nodes, NodeRef targetRef);

size_t telegraf_serialize_node(
    memory_arena* arena, String_Builder* sb, const UaNodeIdExpanded* node);
size_t telegraf_serialize_nodes(
    memory_arena* arena, String_Builder* sb, NodePool* pool, arr_NodeRef* refs);
