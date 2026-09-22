#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "node_pool.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/types.h"
#include "repl.h"

static context ctx;

int main(int argc, char* argv[]) {
    // TODOS:

    printf("sizeof uanode:%zu", sizeof(UaNodeIdExpanded));
    UA_Client* client = UA_Client_new();
    UA_ClientConfig* cc = UA_Client_getConfig(client);
    UA_ClientConfig_setDefault(cc);
    const char* uaEndpoint = "opc.tcp://localhost:4840";
    UA_StatusCode retval = UA_Client_connect(client, uaEndpoint);

    // UA_NodeId root = UA_NODEID_NUMERIC(0, 85);
    ctx.strArena = arena_create(MB(2));
    ctx.tempArena = arena_create(MB(3));

    ctx.client = client;

    // arr_NodeId_Expanded nodes = {0};
    // memset(&ctx.nodes, 0, sizeof(ctx.nodes));
    node_pool_init(&ctx.nodes);

    log_set_level(LOG_TRACE);
    ctx.root = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 85)});
    ctx.current = ctx.root;
    // printPool(&ctx.nodes);
    // NodeRef ref1 =
    //     node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 15)});
    // NodeRef ref2 =
    //     node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 25)});
    // node_pool_remove(&ctx.nodes, ref1);
    // node_pool_remove(&ctx.nodes, ref2);
    // ref1 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 15)});
    // ref2 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 25)});
    // NodeRef ref3 =
    //     node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // printPool(&ctx.nodes);
    // node_pool_remove(&ctx.nodes, ref3);
    // ref1 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 15)});
    // ref2 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 25)});
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref1);
    // node_pool_remove(&ctx.nodes, ref2);
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    //
    // printPool(&ctx.nodes);
    //
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // node_pool_remove(&ctx.nodes, ref3);
    // ref3 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 35)});
    // printPool(&ctx.nodes);

    // u32 idx4 = node_pool_add(&ctx.nodes, (UaNodeIdExpanded){.nodeid = UA_NODEID_NUMERIC(0, 45)});

    String_Builder sb = {0};
    node_pool_get(&ctx.nodes, ctx.current)->explored = true;
    explore_children(ctx.strArena, ctx.client, &ctx.nodes, ctx.root);
    visit_direct_children(ctx.tempArena, &sb, &ctx.nodes, ctx.root, visit_print, NULL);
    String_View sv = sb_to_sv(sb);
    printf(SV_Fmt "\n", SV_Arg(sv));

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
