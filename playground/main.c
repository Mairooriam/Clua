#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log.h>
#include <open62541/types.h>
#include <open62541/util.h>
#include <stdio.h>
#include <string.h>

#include "../clua/src/log.h"

// int mirua_connect(UA_Client* client, const char* endpoint) {
// if (ctx->connected) {
//     log_trace("[CONNECT] - already connected!");
//     return 0;
// }

// TODO: client should be created elsewhere
//  if (!ctx->client) {
//      UA_Client* client = UA_Client_new();
//      if (client) {
//          ctx->client = client;
//      } else {
//          log_warn("[CONNECT] - failed to create client");
//          return 0;
//      }
//  }
//  UA_StatusCode status = -1;
//  if (!endpoint) {
//      log_trace(
//          "[CONNECT] - No endpoint supplied. Using config's endpoint: %s",
//          ctx->config.endpoint);
//      status = UA_Client_connect(ctx->client, ctx->config.endpoint);
//  } else {
//      status = UA_Client_connect(ctx->client, endpoint);
//      if (status == UA_STATUSCODE_GOOD) {
//          free(ctx->config.endpoint);
//          ctx->config.endpoint = strdup(endpoint);
//          log_trace("[CONNECT] - updated endpoint into config");
//      }
//  }

// UA_StatusCode status = UA_Client_connect(client, endpoint);
// if (status == UA_STATUSCODE_GOOD) {
//     log_trace("[CONNECT] - updated endpoint into config");
//
//
//
// if (status == UA_STATUSCODE_GOOD) {
//     ctx->connected = true;
//     log_trace("[CONNECT] - Successfully connected to %s", ctx->config.endpoint);
//
//     // Load all unknown data types from server
//     UA_DataTypeArray* customTypes = NULL;
//     UA_StatusCode dtStatus = UA_Client_getRemoteDataTypes(ctx->client, 0, NULL,
//     &customTypes);
//
//     if (dtStatus != UA_STATUSCODE_GOOD) {
//         log_warn("[CONNECT] Failed to get remote data types: %s",
//         UA_StatusCode_name(dtStatus));
//     } else {
//         if (customTypes && customTypes->typesSize > 0) {
//             log_trace("[CONNECT] Loaded %zu custom data types", customTypes->typesSize);
//
//             // ctx->customDataTypes = customTypes;
//         } else {
//             log_trace("[CONNECT] No custom data types found");
//         }
//     }
//     UA_NodeId_copy(&ctx->config.defaultRoot, &ctx->currentNode.nodeid);
//
//     // Try to browse defaultRoot
//     mirua_t_NodeList testChildren = {0};
//     mirua_init_nodeList(&testChildren, 128);
//     mirua_explore_children(ctx, &testChildren, &ctx->config.defaultRoot);
//
//     if (testChildren.size == 0) {
//         log_warn(
//             "[CONNECT] - config.defaultRoot not found, falling back to OPC UA default root");
//         ctx->config.defaultRoot = UA_NODEID_NUMERIC(0, 85);  // OPC UA standard root folder
//         UA_NodeId_copy(&ctx->config.defaultRoot, &ctx->currentNode.nodeid);
//         mirua_free_nodeList(&testChildren);
//         mirua_init_nodeList(&testChildren, 128);
//         mirua_explore_children(ctx, &testChildren, &ctx->config.defaultRoot);
//     }
//
//     mirua_free_nodeList(&testChildren);
//
//     mirua_history_addToHistory(&ctx->history, &ctx->currentNode);
//     return 1;
// } else {
//     log_warn(
//         "[CONNECT] - to {%s} was not successful", endpoint ? endpoint :
//         ctx->config.endpoint);
//     UA_Client_delete(ctx->client);
//     ctx->client = NULL;
//     return 0;
// }
//
// return 1;
// }

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <test_name>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "1") == 0) {
        UA_Client* client = UA_Client_new();

        const char* endpoint = "opc.tcp://127.0.0.1:4840";

        UA_StatusCode result = UA_Client_connect(client, endpoint);
        if (result != UA_STATUSCODE_GOOD) {
            log_error(
                "Failed to connect to %s with UA_statusCode: %s",
                endpoint,
                UA_StatusCode_name(result));
        } else {
            log_info("Connection to %s was succesfull", endpoint);
        }

        UA_NodeId* node = UA_NodeId_new();
        UA_String str = UA_String_fromChars("ns=6;s=::AsGlobalPV:hmi_pids.EX_LC001");
        UA_NodeId_parse(node, str);

        UA_NodeClass class;
        UA_NodeClass_init(&class);
        UA_StatusCode status = UA_Client_readNodeClassAttribute(client, *node, &class);
        if (status != UA_STATUSCODE_GOOD) {
            log_error("UA_Client_readNodeClassAttribute returned != UA_STATUSCODE_GOOD");
            return false;
        }

        log_info("got NodeClass: %d, expected %d", class, 2);

        log_warn("penis");
    } else if (strcmp(argv[1], "2") == 0) {
        printf("Running Test 2...\n");
    } else if (strcmp(argv[1], "3") == 0) {
        printf("Running Test 3...\n");
    } else {
        printf("Unknown test: %s\n", argv[1]);
        printf("Available tests: test1, test2, test3\n");
    }

    return 0;
}
