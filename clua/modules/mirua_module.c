#include "mirua_module.h"

#include <assert.h>
#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "debug_buf.h"
#include "interpreter.h"
#include "log.h"
#include "mirua_module.h"
#include "mirua_module_internal.h"
#include "mirua_to_string.h"
#include "mirua_types.h"
#include "mirua_types_internal.h"
#include "modules/mirua_serialization.h"
#include "open62541/common.h"
#include "open62541/nodeids.h"

// static void buildNodeIdTree(UA_Client* client, const UA_NodeId* nodeId) {
//     // Browse HasComponent children
//     UA_BrowseRequest bReq;
//     UA_BrowseRequest_init(&bReq);
//     bReq.nodesToBrowseSize = 1;
//     bReq.nodesToBrowse = UA_Array_new(1, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]);
//     UA_BrowseDescription_init(&bReq.nodesToBrowse[0]);
//     UA_NodeId_copy(nodeId, &bReq.nodesToBrowse[0].nodeId);
//     bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
//     bReq.nodesToBrowse[0].referenceTypeId = UA_NODEID_NUMERIC(0, 47);  // HasComponent
//     bReq.nodesToBrowse[0].includeSubtypes = true;
//     bReq.nodesToBrowse[0].nodeClassMask = 0;
//     bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;
//
//     UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
//
//     if (bResp.resultsSize == 1 && bResp.results[0].referencesSize == 0) {
//         // No HasComponent children: this is a leaf node
//         mirua_print_node_path(client, nodeId);
//     } else {
//         for (size_t i = 0; i < bResp.resultsSize; ++i) {
//             for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
//                 UA_ReferenceDescription* ref = &bResp.results[i].references[j];
//                 if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC ||
//                     ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING) {
//                     buildNodeIdTree(client, &ref->nodeId.nodeId);  // Recursive call
//                 }
//             }
//         }
//     }
//     UA_BrowseRequest_clear(&bReq);
//     UA_BrowseResponse_clear(&bResp);
// }

// TODO: if not connected add handling for each method
void mirua_execute(MiruaContext* ctx, ASTNode* node) {
    (void)ctx;
    (void)node;
    // switch (node->command_type) {
    //     case TOKEN_CONNECT: {
    //         mirua_connect(ctx, node);
    //     } break;
    //     case TOKEN_LIST_DIRS: {
    //         mirua_exploreNodes(ctx, node);
    //         mirua_print_current_node(ctx);
    //         mirua_print_current_children(ctx);
    //
    //     } break;
    //     case TOKEN_DISCONNECT: {
    //         UA_Client_disconnect(ctx->client);
    //     } break;
    //     case TOKEN_BROWSE: {
    //         UA_NodeId browseNode = UA_NODEID_NUMERIC(5, 100000);
    //
    //         // UA_StatusCode status =
    //         UA_Client_forEachChildNodeCall(ctx->client, browseNode, mirua_cb_printNodeEx,
    //         ctx->client); mirua_print_node_path(ctx->client, &browseNode); UA_NodeId nodeId =
    //         UA_NODEID_STRING(5, "::Program1:transform"); mirua_printNodeDataType(ctx->client,
    //         &nodeId);
    //         // or UA_NodeId nodeId = UA_NODEID_NUMERIC(5, 100000);
    //         mirua_print_node_value_json(ctx->client, &nodeId);
    //         // TODO: epxlore further with UA_Client_getRemoteDataTypes and see if it reads the
    //         transforms datatype which
    //         // is sturcture
    //         //  ->> printing
    //         UA_NodeId_clear(&browseNode);
    //     } break;
    //     case TOKEN_CURRENT: {
    //         char* buf = dbg_getbuf(0);
    //         size_t bufsize = dbg_buf_size();
    //         mirua_to_string_nodeIdEx(buf, bufsize, ctx->client, &ctx->currentNode);
    //         printf("[CUREENT NODE] - %s\n", buf);
    //
    //         buildNodeIdTree(ctx->client, &ctx->currentNode);
    //
    //     } break;
    //
    //     case TOKEN_NAVIGATE_DOWN: {
    //         printf("NAVIGATE_DOWN triggered\n");
    //         size_t idx = 0;
    //         const char* param0 = NULL;
    //         const char* param1 = NULL;
    //
    //         if (node->child_count > 0 && node->children[0]->type == AST_PARAMETER) {
    //             param0 = node->children[0]->data.parameter.value;
    //         }
    //         if (node->child_count > 1 && node->children[1]->type == AST_PARAMETER) {
    //             param1 = node->children[1]->data.parameter.value;
    //         }
    //
    //         char* endptr;
    //         long parsedIdx = param0 ? strtol(param0, &endptr, 10) : -1;
    //         if (!param0 || endptr == param0 || *endptr != '\0') {
    //             log_warn("[NAVIGATE] - Invalid child index: %s", param0 ? param0 : "(null)");
    //         } else if (!param1 || strlen(param1) == 0) {
    //             idx = (size_t)parsedIdx;
    //             if (idx < ctx->currentChildren.size) {
    //                 UA_NodeId_clear(&ctx->currentNode);
    //                 UA_NodeId_copy(&ctx->currentChildren.nodeIds[idx], &ctx->currentNode);
    //                 mirua_history_addToHistory(&ctx->history, &ctx->currentNode);
    //                 mirua_exploreNodes(ctx, NULL);
    //             } else {
    //                 log_warn("[NAVIGATE] - Invalid child index: %zu", idx);
    //             }
    //         } else if (strcmp(param1, "datatype") == 0 || strcmp(param1, "d") == 0) {
    //             idx = (size_t)parsedIdx;
    //             if (idx < ctx->currentChildren.size) {
    //                 UA_NodeId_clear(&ctx->currentNode);
    //                 UA_NodeId datatypeNodeId;
    //                 UA_NodeId_init(&datatypeNodeId);
    //                 UA_Client_readDataTypeAttribute(ctx->client,
    //                 ctx->currentChildren.nodeIds[idx], &datatypeNodeId);
    //                 UA_NodeId_copy(&datatypeNodeId, &ctx->currentNode);
    //                 mirua_history_addToHistory(&ctx->history, &ctx->currentNode);
    //                 mirua_exploreNodes(ctx, NULL);
    //             } else {
    //                 log_warn("[NAVIGATE] - Invalid child index: %zu", idx);
    //             }
    //         } else {
    //             log_warn("[NAVIGATE] - Unknown parameter combination: %s, %s", param0, param1);
    //         }
    //
    //         mirua_print_current_node(ctx);
    //         mirua_print_current_children(ctx);
    //     } break;
    //     case TOKEN_NAVIGATE_UP: {  // TODO: currently doesnt go up to the start node. histroy not
    //     being updated
    //                                // initally. maybe initialize as root folder in ctx?
    //         if (mirua_history_goBack(&ctx->history)) {
    //             ctx->currentNode = ctx->history.nodeIds[ctx->history.count - 1];
    //             mirua_exploreNodes(ctx, NULL);
    //         }
    //
    //         mirua_print_current_children(ctx);
    //         break;
    //     }
    //     case TOKEN_SETTINGS: {
    //         printf("Available settings\n");
    //         printf("Current print level: [%i]\n", ctx->nodeIdPrintLevel);
    //         printf("[ROOT NODE] - ");
    //         ast_print_debug_node(node);
    //         printf("\n");
    //         for (size_t i = 0; i < node->child_count;
    //              ++i) {  // TODO: add handling for set nodeidprintlevel 1 -> currently just takes
    //              1 number and changes
    //                      // it not dynami,c
    //             ASTNode* child = node->children[i];
    //             char* end;
    //             int val = -1;
    //             val = strtol(child->data.parameter.value, &end, 10);
    //             if (child->command_type == TOKEN_NUMBER) {
    //                 if (val >= 0 && val <= 3) {
    //                     ctx->nodeIdPrintLevel = val;
    //                     printf("NodeIdPrintLevel set to %i\n", val);
    //                     break;
    //                 }
    //             }
    //             printf("[CHILD NDOE {%zu}] Node: ", i);
    //             ast_print_debug_node(node);
    //             printf("[TOKEN TYPE]: %s\n", lexer_to_string_tokentype(child->command_type));
    //         }
    //     } break;
    //     case TOKEN_HISTORY: {
    //         char* buf = dbg_getbuf(0);
    //         size_t bufsize = dbg_buf_size();
    //         switch (
    //             ctx->nodeIdPrintLevel) {  // TODO:: ADD to context "NodeId_print_func" to get rid
    //             of multiple switches case 0: { } break; case 1: { } break; case 2: { } break;
    //         }
    //         mirua_history_to_string(buf, bufsize, ctx->client, &ctx->history,
    //         mirua_to_string_nodeId); printf("[HISTORY]:\n %s\n", buf);
    //     } break;
    //     case TOKEN_SAVE: {

    //     } break;
    //     default: {
    //     } break;
    //         // case TOKEN_CONNECT: {} break;
    //         // case TOKEN_CONNECT: {} break;
    //         // case TOKEN_CONNECT: {} break;
    //         // case TOKEN_CONNECT: {} break;
    // }
}
void mirua_save(MiruaContext* ctx, int start, int end) {
    // GETTING RANGE OF NODES 
    mirua_t_NodeList subset;
    size_t range_size = (end >= start && (size_t)end < ctx->currentChildren.size) ? (size_t)(end - start + 1) : 0;
    mirua_init_nodeList(&subset, range_size);

    for (size_t i = (size_t)start; i <= (size_t)end && i < ctx->currentChildren.size; i++) {
        if (subset.size >= subset.capacity) {
            size_t new_cap = subset.capacity * 2;
            MiruaNodeId* new_nodes = realloc(subset.nodeIds, sizeof(MiruaNodeId) * new_cap);
            if (!new_nodes) {
                log_error("[SAVE] Failed to resize subset list");
                mirua_free_nodeList(&subset);
                return;
            }
            subset.nodeIds = new_nodes;
            subset.capacity = new_cap;
        }
        mirua_nodeId_init(&subset.nodeIds[subset.size]);
        mirua_nodeId_copy(&ctx->currentChildren.nodeIds[i], &subset.nodeIds[subset.size]);
        subset.size++;
    }


    // FILE HANDLING
    const char* filename = ctx->config.output_path ? ctx->config.output_path : "output_file.txt";
    bool filename_allocated = false;
    const char* mode = "w";
    FILE* check_file = fopen(filename, "r");
    if (check_file) {
        fclose(check_file);
        printf("File '%s' already exists. Overwrite (o), Append (a), or New name (n)? ", filename);
        char response = getchar();
        while (getchar() != '\n');  // Consume newline
        if (response == 'a' || response == 'A') {
            mode = "a";
        } else if (response == 'n' || response == 'N') {
            printf("Enter new filename: ");
            char new_filename[256];
            if (fgets(new_filename, sizeof(new_filename), stdin)) {
                new_filename[strcspn(new_filename, "\n")] = '\0';
                filename = strdup(new_filename);
                if (!filename) {
                    log_error("[SAVE] Failed to allocate memory for filename");
                    mirua_free_nodeList(&subset);
                    return;
                }
                filename_allocated = true;
            } else {
                printf("Invalid input, using default.\n");
            }
        } else if (response != 'o' && response != 'O') {
            printf("Invalid choice, using default (overwrite).\n");
        }
    }

    FILE* f = fopen(filename, mode);
    if (!f) {
        log_error("[SAVE] Failed to open file '%s' for %s", filename, mode[0] == 'a' ? "appending" : "writing");
        if (filename_allocated) free((char*)filename);
        mirua_free_nodeList(&subset);
        return;
    }

    // SERIALIZING
    log_trace("[SAVE] Serializing children from %d to %d to '%s' (%s)", start, end - 1, filename, mode[0] == 'a' ? "appending" : "overwriting");
    MiruaNodeSerializer* serializer = mirua_serializer_get(SERIALIZER_TELEGRF);
    if (serializer->serialize_nodes_to_file(&subset, f) == SIZE_MAX) {
        log_error("[SAVE] Serialization failed or buffer overflow");
    } else {
        log_trace("[SAVE] Successfully %s to '%s'", mode[0] == 'a' ? "appended" : "saved", filename);
    }

    fclose(f);
    if (filename_allocated) free((char*)filename);
    mirua_free_nodeList(&subset);
}
MiruaState mirua_state_get(MiruaContext* ctx) {
    return ctx->state;
}

// TODO: rename this set
void mirua_state_change(MiruaContext* ctx, MiruaState newState) {
    log_trace(
        "[STATE] - changed from (%s) to (%s)",
        mirua_state_to_string(ctx->state),
        mirua_state_to_string(newState));
    ctx->state = newState;
}

bool mirua_callback_filter_ALL(
    UA_NodeId childId, UA_NodeId referenceTypeId, UA_Client* client, void* data) {
    (void)childId;
    (void)referenceTypeId;
    (void)client;
    (void)data;
    return true;
}

bool mirua_callback_filter(
    UA_NodeId childId, UA_NodeId referenceTypeId, UA_Client* client, void* data) {
    (void)referenceTypeId;
    MiruaContext* ctx = (MiruaContext*)data;
    UA_Variant var;
    UA_Variant_init(&var);

    UA_StatusCode status = UA_Client_readValueAttribute(client, childId, &var);
    if (status != UA_STATUSCODE_GOOD || !var.type) {
        UA_Variant_clear(&var);
        return false;
    }

    UA_DataTypeKind kind = var.type->typeKind;
    bool allowed = (ctx->config.selectedDataTypeKinds == 0) ||
        (ctx->config.selectedDataTypeKinds & (1U << kind));

    UA_Variant_clear(&var);
    return allowed;
}

MiruaNodeFilter mirua_filter_get_func(MiruaFilterType type) {
    switch (type) {
        case MIRUA_FILTER_ALL: return mirua_callback_filter_ALL;
        case MIRUA_FILTER_UA_TYPES: return mirua_callback_filter;
        default: return NULL;
    }
}

const char* mirua_filter_get_name(MiruaFilterType type) {
    switch (type) {
        case MIRUA_FILTER_ALL: return "ALL";
        case MIRUA_FILTER_UA_TYPES: return "UA_TYPES";
        default: return "UNKNOWN";
    }
}

MiruaFilterType mirua_filter_parse_type(const char* str) {
    if (strcmp(str, "ALL") == 0) {
        return MIRUA_FILTER_ALL;
    } else if (strcmp(str, "UA") == 0) {
        return MIRUA_FILTER_UA_TYPES;
    } else {
        log_error("[FILTER PARSE] - invalid filter type string");
        return MIRUA_FILTER_INVALID;
    }
}

MiruaContext* mirua_module_create(void) {
    MiruaContext* ctx = malloc(sizeof(MiruaContext));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(MiruaContext));  // This zeroes all fields
    ctx->config.filterType = MIRUA_FILTER_ALL;
    UA_NodeId_init(&ctx->currentNode.nodeid);
    mirua_init_nodeList(&ctx->currentChildren, 128);

    // char* endpoint = read_endpoint_from_file("mirua_config.ini"); // TODO: config impelmentaion
    //  if (endpoint) {:356
    //
    //      ctx->config.endpoint = endpoint;
    //  } else {
    ctx->config.endpoint = strdup("opc.tcp://127.0.0.1:4840");
    ctx->config.defaultRoot = UA_NODEID_NUMERIC(0, 85);
    ctx->config.output_path = "output.txt";

    // }
    ctx->config.selectedDataTypeKinds = DEFAULT_DATATYPE_FILTER_MASK;
    ctx->state = MIRUA_STATE_NORMAL;
    return ctx;
}
void mirua_module_free(MiruaContext* ctx) {
    if (ctx->connected) {
        UA_Client_disconnect(ctx->client);
    }

    UA_Client_delete(ctx->client);
    UA_ClientConfig_delete(ctx->ua_config);
}

int mirua_connect(MiruaContext* ctx, const char* endpoint) {
    // if (validEndpoint()) //TODO: checking for valid endpoint
    // {
    //     ctx->config.endpoint = endpoint;
    // }

    if (ctx->connected) {
        log_trace("[CONNECT] - already connected!");
        return 0;
    }

    if (!ctx->client) {
        UA_Client* client = UA_Client_new();
        if (client) {
            ctx->client = client;
        } else {
            log_warn("[CONNECT] - failed to create client");
            return 0;
        }
    }
    UA_StatusCode status = -1;
    if (!endpoint) {
        log_trace(
            "[CONNECT] - No endpoint supplied. Using config's endpoint: %s", ctx->config.endpoint);
        status = UA_Client_connect(ctx->client, ctx->config.endpoint);
    } else {
        status = UA_Client_connect(ctx->client, endpoint);
        if (status == UA_STATUSCODE_GOOD) {
            free(ctx->config.endpoint);
            ctx->config.endpoint = strdup(endpoint);
            log_trace("[CONNECT] - updated endpoint into config");
        }
    }

    if (status == UA_STATUSCODE_GOOD) {
        ctx->connected = true;
        log_trace("[CONNECT] - Successfully connected to %s", ctx->config.endpoint);

        // Load all unknown data types from server
        UA_DataTypeArray* customTypes = NULL;
        UA_StatusCode dtStatus = UA_Client_getRemoteDataTypes(ctx->client, 0, NULL, &customTypes);

        if (dtStatus != UA_STATUSCODE_GOOD) {
            log_warn("[CONNECT] Failed to get remote data types: %s", UA_StatusCode_name(dtStatus));
        } else {
            if (customTypes && customTypes->typesSize > 0) {
                log_trace("[CONNECT] Loaded %zu custom data types", customTypes->typesSize);

                // ctx->customDataTypes = customTypes;
            } else {
                log_trace("[CONNECT] No custom data types found");
            }
        }
        UA_NodeId_copy(&ctx->config.defaultRoot, &ctx->currentNode.nodeid);

        // Try to browse defaultRoot
        mirua_t_NodeList testChildren = {0};
        mirua_init_nodeList(&testChildren, 128);
        mirua_explore_children(ctx, &testChildren, &ctx->config.defaultRoot);

        if (testChildren.size == 0) {
            log_warn(
                "[CONNECT] - config.defaultRoot not found, falling back to OPC UA default root");
            ctx->config.defaultRoot = UA_NODEID_NUMERIC(0, 85);  // OPC UA standard root folder
            UA_NodeId_copy(&ctx->config.defaultRoot, &ctx->currentNode.nodeid);
            mirua_free_nodeList(&testChildren);
            mirua_init_nodeList(&testChildren, 128);
            mirua_explore_children(ctx, &testChildren, &ctx->config.defaultRoot);
        }

        mirua_free_nodeList(&testChildren);

        mirua_history_addToHistory(&ctx->history, &ctx->currentNode);
        return 1;
    } else {
        log_warn(
            "[CONNECT] - to {%s} was not successful", endpoint ? endpoint : ctx->config.endpoint);
        UA_Client_delete(ctx->client);
        ctx->client = NULL;
        return 0;
    }

    return 1;
}

void mirua_explore_children(MiruaContext* ctx, mirua_t_NodeList* nodes, const UA_NodeId* node) {
    mirua_clear_nodeList(nodes);

    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse = UA_Array_new(1, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]);
    UA_BrowseDescription_init(&bReq.nodesToBrowse[0]);
    UA_NodeId_copy(node, &bReq.nodesToBrowse[0].nodeId);
    bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bReq.nodesToBrowse[0].referenceTypeId = UA_NODEID_NUMERIC(
        0, UA_NS0ID_HIERARCHICALREFERENCES);  // Hierarchical references (includes HasComponent)
    bReq.nodesToBrowse[0].includeSubtypes = true;
    bReq.nodesToBrowse[0].nodeClassMask = 0;
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

    UA_BrowseResponse bResp = UA_Client_Service_browse(ctx->client, bReq);

    // Process results
    if (bResp.resultsSize > 0) {
        for (size_t i = 0; i < bResp.results[0].referencesSize; ++i) {
            UA_ReferenceDescription* ref = &bResp.results[0].references[i];

            MiruaNodeFilter filter = mirua_filter_get_func(ctx->config.filterType);
            if (filter && !filter(ref->nodeId.nodeId, ref->referenceTypeId, ctx->client, ctx)) {
                continue;
            }

            if (nodes->size >= nodes->capacity) {
                size_t new_cap = nodes->capacity * 2;
                MiruaNodeId* new_nodes = realloc(nodes->nodeIds, sizeof(MiruaNodeId) * new_cap);
                if (!new_nodes) {
                    log_error("[EXPLORE] Failed to resize node list");
                    break;
                }
                nodes->nodeIds = new_nodes;
                nodes->capacity = new_cap;
            }

            // Initialize and copy the node
            mirua_nodeId_init(&nodes->nodeIds[nodes->size]);
            UA_NodeId_copy(&ref->nodeId.nodeId, &nodes->nodeIds[nodes->size].nodeid);
            UA_QualifiedName_copy(&ref->browseName, &nodes->nodeIds[nodes->size].name);
            nodes->size++;
        }
    } else {
        log_warn("[EXPLORE] Browse failed or no results for node");
    }

    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);

    // Testing raw brwose from open65421 for easier addition of stuff
    //  mirua_clear_nodeList(nodes);

    // MiruaNodeFilter filter = mirua_filter_get_func(ctx->config.filterType);
    // MiruaCallbackHandle handle = {
    //     .nodes = nodes, .client = ctx->client, .filter = filter, .userData = ctx};

    // UA_Client_forEachChildNodeCall(ctx->client, *node, mirua_cb_collectNodes_to_nodelist,
    // &handle);
}

bool mirua_node_exists(UA_Client* client, const UA_NodeId* node) {
    UA_NodeClass nodeClass;
    UA_StatusCode status = UA_Client_readNodeClassAttribute(client, *node, &nodeClass);

    if (status == UA_STATUSCODE_GOOD) {
        log_trace(
            "[NODE_EXISTS] Node exists: ns=%u;i=%u",
            node->namespaceIndex,
            node->identifier.numeric);
        return true;
    } else {
        log_warn(
            "[NODE_EXISTS] Node does not exist or read failed: %s", UA_StatusCode_name(status));
        return false;
    }
}
void mirua_exploreNodes(MiruaContext* ctx, const char* node, int idx) {
    if (ctx->currentChildren.nodeIds == NULL || ctx->currentChildren.size == 0) {
        mirua_explore_children(ctx, &ctx->currentChildren, &ctx->config.defaultRoot);
        UA_NodeId_copy(&ctx->config.defaultRoot, &ctx->currentNode.nodeid);
        mirua_print_current_node(ctx);
        mirua_print_current_children(ctx);
        return;
    }

    if (idx >= 0) {
        if ((size_t)idx < ctx->currentChildren.size) {
            mirua_t_NodeList nodes = {0};
            mirua_explore_children(ctx, &nodes, &ctx->currentChildren.nodeIds[idx].nodeid);
            mirua_print_node(ctx, &ctx->currentChildren.nodeIds[idx].nodeid, 0);
            for (size_t i = 0; i < nodes.size; i++) {
                mirua_print_node(ctx, &nodes.nodeIds[i].nodeid, 1);
            }
            mirua_free_nodeList(&nodes);
        }
    } else if (node && strlen(node) > 0) {
        UA_NodeId targetNode;
        UA_NodeId_init(&targetNode);
        UA_String str = UA_String_fromChars(node);
        UA_StatusCode status = UA_NodeId_parse(&targetNode, str);

        if (status == UA_STATUSCODE_GOOD && mirua_node_exists(ctx->client, &targetNode)) {
            mirua_t_NodeList nodes = {0};
            mirua_explore_children(ctx, &nodes, &targetNode);
            mirua_print_node(ctx, &targetNode, 0);
            for (size_t i = 0; i < nodes.size; i++) {
                mirua_print_node(ctx, &nodes.nodeIds[i].nodeid, 1);
            }
            mirua_free_nodeList(&nodes);
        } else {
            log_warn("[EXPLORE] - Invalid or non-existent node: %s", node);
        }
        UA_String_clear(&str);
        UA_NodeId_clear(&targetNode);
    } else {
        mirua_explore_children(ctx, &ctx->currentChildren, &ctx->currentNode.nodeid);
        mirua_print_current_node(ctx);
        mirua_print_current_children(ctx);
    }
}
int mirua_navigate_down(MiruaContext* ctx, size_t idx) {
    printf("NAVIGATE_DOWN triggered\n");

    if (idx >= ctx->currentChildren.size) {
        log_warn("[NAVIGATE] - Invalid child index: %zu", idx);
        mirua_print_current_node(ctx);
        mirua_print_current_children(ctx);
        return -1;
    }

    UA_NodeId_clear(&ctx->currentNode.nodeid);
    UA_NodeId_copy(&ctx->currentChildren.nodeIds[idx].nodeid, &ctx->currentNode.nodeid);
    mirua_history_addToHistory(&ctx->history, &ctx->currentNode);

    // TODO: navigate datatype of a node
    //  UA_NodeId datatypeNodeId;
    //  UA_NodeId_init(&datatypeNodeId);
    //  UA_Client_readDataTypeAttribute(
    //      ctx->client, ctx->currentChildren.nodeIds[idx], &datatypeNodeId);
    //  UA_NodeId_copy(&datatypeNodeId, &ctx->currentNode);
    //  mirua_history_addToHistory(&ctx->history, &ctx->currentNode);

    mirua_explore_children(ctx, &ctx->currentChildren, &ctx->currentNode.nodeid);

    mirua_print_current_node(ctx);
    mirua_print_current_children(ctx);
    return 0;
}
int mirua_navigate_up(MiruaContext* ctx) {
    if (mirua_history_goBack(&ctx->history)) {
        ctx->currentNode = ctx->history.nodeIds[ctx->history.count - 1];
        mirua_explore_children(ctx, &ctx->currentChildren, &ctx->currentNode.nodeid);
    }
    mirua_print_current_node(ctx);
    mirua_print_current_children(ctx);

    return 0;
}
int mirua_navigate_to(MiruaContext* ctx, const char* nodeid_str) {
    UA_String str = UA_String_fromChars(nodeid_str);
    UA_NodeId node;
    UA_NodeId_init(&node);
    UA_StatusCode status = UA_NodeId_parse(&node, str);
    if (status == UA_STATUSCODE_GOOD) {
        if (mirua_node_exists(ctx->client, &node)) {
            UA_NodeId_clear(&ctx->currentNode.nodeid);
            UA_NodeId_copy(&node, &ctx->currentNode.nodeid);
            mirua_history_addToHistory(&ctx->history, &ctx->currentNode);
            mirua_explore_children(ctx, &ctx->currentChildren, &ctx->currentNode.nodeid);
        } else {
            log_warn("[NAVIGATE_TO] - %s does not exist on the target server.", nodeid_str);
            return -1;
        }

    } else {
        log_warn("[NAVIGATE_TO] Failed to parse %s into nodeid", nodeid_str);
        return -1;
    }
    return 0;
}

UA_StatusCode mirua_cb_printNode(
    UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle) {
    (void)isInverse;
    (void)referenceTypeId;
    (void)handle;

    UA_String str;
    UA_String_init(&str);
    UA_NodeId_print(&childId, &str);
    printf("Child NodeId: %.*s\n", (int)str.length, str.data);
    UA_String_clear(&str);
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode mirua_cb_printNodeEx(
    UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle) {
    (void)isInverse;
    (void)referenceTypeId;
    (void)handle;

    UA_Client* client = (UA_Client*)handle;  // Cast handle back to UA_Client*
    UA_QualifiedName browseName;
    UA_QualifiedName_init(&browseName);

    // Read the BrowseName attribute of the node
    UA_StatusCode retval = UA_Client_readBrowseNameAttribute(client, childId, &browseName);
    if (retval == UA_STATUSCODE_GOOD) {
        printf(
            "NodeId: ns=%u;i=%u, BrowseName: %.*s\n",
            childId.namespaceIndex,
            childId.identifier.numeric,
            (int)browseName.name.length,
            browseName.name.data);
    } else {
        printf(
            "NodeId: ns=%u;i=%u, BrowseName: <read failed>\n",
            childId.namespaceIndex,
            childId.identifier.numeric);
    }
    UA_QualifiedName_clear(&browseName);
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode mirua_cb_collectNodes_to_nodelist(
    UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle) {
    (void)referenceTypeId;
    if (isInverse) return UA_STATUSCODE_GOOD;

    MiruaCallbackHandle* h = (MiruaCallbackHandle*)handle;
    mirua_t_NodeList* nodes = h->nodes;
    // mirua_t_NodeList* nodes = (mirua_t_NodeList*)handle;
    if (h->filter && !h->filter(childId, referenceTypeId, h->client, h->userData)) {
        return UA_STATUSCODE_GOOD;
    }

    if (nodes->size < nodes->capacity) {
        UA_NodeId_copy(&childId, &nodes->nodeIds[nodes->size].nodeid);
    }
    nodes->size++;
    return UA_STATUSCODE_GOOD;
}
UA_StatusCode mirua_cb_nodeIter(
    UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle) {
    if (isInverse) return UA_STATUSCODE_GOOD;
    UA_NodeId* parent = (UA_NodeId*)handle;
    printf(
        "%u, %u --- %u ---> NodeId %u, %u\n",
        parent->namespaceIndex,
        parent->identifier.numeric,
        referenceTypeId.identifier.numeric,
        childId.namespaceIndex,
        childId.identifier.numeric);
    return UA_STATUSCODE_GOOD;
}

void mirua_history_addToHistory(NodeIdHistory* hist, const MiruaNodeId* nodeId) {
    if (hist->count < MAX_HISTORY) {
        mirua_nodeId_copy(nodeId, &hist->nodeIds[hist->count]);
        hist->count++;
    }
}

int mirua_history_goBack(NodeIdHistory* hist) {
    if (hist->count > 1) {
        hist->count--;
        return 1;
    }
    return 0;
}

size_t mirua_to_string_nodeId(char* buf, size_t bufsize, UA_Client* client, const UA_NodeId* node) {
    size_t offset = 0;
    switch (node->identifierType) {
        case UA_NODEIDTYPE_NUMERIC:
            offset += snprintf(
                buf, bufsize, "ns=%u;i=%u", node->namespaceIndex, node->identifier.numeric);
            break;
        case UA_NODEIDTYPE_STRING:
            offset += snprintf(
                buf,
                bufsize,
                "ns=%u;s=%.*s",
                node->namespaceIndex,
                (int)node->identifier.string.length,
                node->identifier.string.data);
            break;
        case UA_NODEIDTYPE_GUID: {
            const UA_Guid* g = &node->identifier.guid;
            offset += snprintf(
                buf,
                bufsize,
                "ns=%u;g=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                node->namespaceIndex,
                g->data1,
                g->data2,
                g->data3,
                g->data4[0],
                g->data4[1],
                g->data4[2],
                g->data4[3],
                g->data4[4],
                g->data4[5],
                g->data4[6],
                g->data4[7]);
            break;
        }
        case UA_NODEIDTYPE_BYTESTRING:
            offset +=
                snprintf(buf, bufsize, "ns=%u;b=...", node->namespaceIndex);  // Implement as needed
            break;
        default: offset += snprintf(buf, bufsize, "ns=%u;unknown", node->namespaceIndex); break;
    }

    // Add BrowseName
    UA_QualifiedName browseName;
    UA_QualifiedName_init(&browseName);
    UA_StatusCode retval = UA_Client_readBrowseNameAttribute(client, *node, &browseName);
    if (retval == UA_STATUSCODE_GOOD) {
        offset += snprintf(
            buf + offset,
            bufsize - offset,
            "  -  [BrowseName: %.*s]",
            (int)browseName.name.length,
            browseName.name.data);
    } else {
        offset += snprintf(buf + offset, bufsize - offset, "  -  [BrowseName: <read failed>]");
    }
    UA_QualifiedName_clear(&browseName);

    return offset;
}
size_t mirua_to_string_nodeIdEx(
    char* buf, size_t bufsize, UA_Client* client, const UA_NodeId* node) {
    size_t offset = 0;
    switch (node->identifierType) {
        case UA_NODEIDTYPE_NUMERIC:
            offset += snprintf(
                buf, bufsize, "ns=%u;i=%u", node->namespaceIndex, node->identifier.numeric);
            break;
        case UA_NODEIDTYPE_STRING:
            offset += snprintf(
                buf,
                bufsize,
                "ns=%u;s=%.*s",
                node->namespaceIndex,
                (int)node->identifier.string.length,
                node->identifier.string.data);
            break;
        case UA_NODEIDTYPE_GUID: {
            const UA_Guid* g = &node->identifier.guid;
            offset += snprintf(
                buf,
                bufsize,
                "ns=%u;g=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                node->namespaceIndex,
                g->data1,
                g->data2,
                g->data3,
                g->data4[0],
                g->data4[1],
                g->data4[2],
                g->data4[3],
                g->data4[4],
                g->data4[5],
                g->data4[6],
                g->data4[7]);
            break;
        }
        case UA_NODEIDTYPE_BYTESTRING:
            offset += snprintf(
                buf,
                bufsize,
                "ns=%u;b=...NOTIMPLEMENTED",
                node->namespaceIndex);  // Implement as needed
            break;
        default: offset += snprintf(buf, bufsize, "ns=%u;unknown", node->namespaceIndex); break;
    }

    int size = 5;
    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    request.nodesToReadSize = size;
    request.nodesToRead = UA_Array_new(size, &UA_TYPES[UA_TYPES_READVALUEID]);

    UA_ReadValueId_init(&request.nodesToRead[0]);
    UA_NodeId_copy(node, &request.nodesToRead[0].nodeId);
    request.nodesToRead[0].attributeId = UA_ATTRIBUTEID_NODECLASS;

    UA_ReadValueId_init(&request.nodesToRead[1]);
    UA_NodeId_copy(node, &request.nodesToRead[1].nodeId);
    request.nodesToRead[1].attributeId = UA_ATTRIBUTEID_BROWSENAME;

    UA_ReadValueId_init(&request.nodesToRead[2]);
    UA_NodeId_copy(node, &request.nodesToRead[2].nodeId);
    request.nodesToRead[2].attributeId = UA_ATTRIBUTEID_DISPLAYNAME;

    UA_ReadValueId_init(&request.nodesToRead[3]);
    UA_NodeId_copy(node, &request.nodesToRead[3].nodeId);
    request.nodesToRead[3].attributeId = UA_ATTRIBUTEID_DESCRIPTION;

    UA_ReadValueId_init(&request.nodesToRead[4]);
    UA_NodeId_copy(node, &request.nodesToRead[4].nodeId);
    request.nodesToRead[4].attributeId = UA_ATTRIBUTEID_VALUE;

    UA_ReadResponse response = UA_Client_Service_read(client, request);
    char indent[64] = {0};
    for (size_t i = 0; i < response.resultsSize; ++i) {
        if (response.results[i].hasValue) {
            UA_EncodeJsonOptions options;
            memset(&options, 0, sizeof(options));
            options.prettyPrint = false;   // Human-readable
            options.unquotedKeys = false;  // Standard JSON
            options.useReversible = true;  // Not necessarily reversible
            options.stringNodeIds = true;  // NodeIds as strings

            UA_String str;
            UA_String_init(&str);
            const char* attributeid;
            attributeid = mirua_to_string_attribute_id(request.nodesToRead[i].attributeId);

            // --- ExtensionObject handling ---
            if (response.results[i].value.type == &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]) {
                UA_ExtensionObject* eo = (UA_ExtensionObject*)response.results[i].value.data;
                if (eo->encoding == UA_EXTENSIONOBJECT_DECODED && eo->content.decoded.type) {
                    UA_String extStr;
                    UA_String_init(&extStr);
                    UA_encodeJson(
                        eo->content.decoded.data, eo->content.decoded.type, &extStr, &options);
                    offset += snprintf(
                        buf + offset,
                        bufsize - offset,
                        "\n%s AttributeId: %s ExtensionObject: %.*s",
                        indent,
                        attributeid,
                        (int)extStr.length,
                        extStr.data);
                    UA_String_clear(&extStr);
                } else {
                    offset += snprintf(
                        buf + offset,
                        bufsize - offset,
                        "\n%s AttributeId: %s ExtensionObject: <not decoded or unknown type>",
                        indent,
                        attributeid);
                }
            } else {
                // --- Normal value handling ---
                UA_encodeJson(
                    response.results[i].value.data, response.results[i].value.type, &str, &options);
                offset += snprintf(
                    buf + offset,
                    bufsize - offset,
                    "\n%s AttributeId: %s val:%.*s",
                    indent,
                    attributeid,
                    (int)str.length,
                    str.data);
            }
            UA_String_clear(&str);
        }
    }

    UA_ReadRequest_clear(&request);
    UA_ReadResponse_clear(&response);
    return 0;  // TODO: handle error codes for return
}

// typedef enum {
//     UA_ATTRIBUTEID_NODEID                  = 1,
//     UA_ATTRIBUTEID_NODECLASS               = 2,
//     UA_ATTRIBUTEID_BROWSENAME              = 3,
//     UA_ATTRIBUTEID_DISPLAYNAME             = 4,
//     UA_ATTRIBUTEID_DESCRIPTION             = 5,
//     UA_ATTRIBUTEID_WRITEMASK               = 6,
//     UA_ATTRIBUTEID_USERWRITEMASK           = 7,
//     UA_ATTRIBUTEID_ISABSTRACT              = 8,
//     UA_ATTRIBUTEID_SYMMETRIC               = 9,
//     UA_ATTRIBUTEID_INVERSENAME             = 10,
//     UA_ATTRIBUTEID_CONTAINSNOLOOPS         = 11,
//     UA_ATTRIBUTEID_EVENTNOTIFIER           = 12,
//     UA_ATTRIBUTEID_VALUE                   = 13,
//    UA_ATTRIBUTEID_DATATYPE                = 14,
//     UA_ATTRIBUTEID_VALUERANK               = 15,
//     UA_ATTRIBUTEID_ARRAYDIMENSIONS         = 16,
//     UA_ATTRIBUTEID_ACCESSLEVEL             = 17,
//     UA_ATTRIBUTEID_USERACCESSLEVEL         = 18,
//     UA_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL = 19,
//     UA_ATTRIBUTEID_HISTORIZING             = 20,
//     UA_ATTRIBUTEID_EXECUTABLE              = 21,
//     UA_ATTRIBUTEID_USEREXECUTABLE          = 22,
//     UA_ATTRIBUTEID_DATATYPEDEFINITION      = 23,
//     UA_ATTRIBUTEID_ROLEPERMISSIONS         = 24,
//     UA_ATTRIBUTEID_USERROLEPERMISSIONS     = 25,
//     UA_ATTRIBUTEID_ACCESSRESTRICTIONS      = 26,
//     UA_ATTRIBUTEID_ACCESSLEVELEX           = 27
// } UA_AttributeId;
// UA_UINT32
//
void mirua_printNodeDataType(UA_Client* client, const UA_NodeId* nodeId) {
    // Read DataType attribute
    UA_ReadRequest req;
    UA_ReadRequest_init(&req);
    req.nodesToReadSize = 1;
    req.nodesToRead = UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
    UA_ReadValueId_init(&req.nodesToRead[0]);
    UA_NodeId_copy(nodeId, &req.nodesToRead[0].nodeId);
    req.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DATATYPE;

    UA_ReadResponse resp = UA_Client_Service_read(client, req);

    if (resp.resultsSize == 1 && resp.results[0].hasValue &&
        UA_Variant_hasScalarType(&resp.results[0].value, &UA_TYPES[UA_TYPES_NODEID])) {
        UA_NodeId* dataTypeId = (UA_NodeId*)resp.results[0].value.data;
        printf(
            "DataType NodeId: ns=%u;i=%u\n",
            dataTypeId->namespaceIndex,
            dataTypeId->identifier.numeric);

        // Read DataTypeDefinition attribute of the DataType node
        UA_ReadRequest req2;
        UA_ReadRequest_init(&req2);
        req2.nodesToReadSize = 1;
        req2.nodesToRead = UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
        UA_ReadValueId_init(&req2.nodesToRead[0]);
        UA_NodeId_copy(dataTypeId, &req2.nodesToRead[0].nodeId);
        req2.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DATATYPEDEFINITION;

        UA_ReadResponse resp2 = UA_Client_Service_read(client, req2);

        if (resp2.resultsSize == 1 && resp2.results[0].hasValue) {
            UA_EncodeJsonOptions options;
            memset(&options, 0, sizeof(options));
            options.prettyPrint = true;
            UA_String json;
            UA_String_init(&json);
            UA_encodeJson(
                resp2.results[0].value.data, resp2.results[0].value.type, &json, &options);
            printf("DataTypeDefinition: %.*s\n", (int)json.length, json.data);
            UA_String_clear(&json);
        } else {
            printf("Failed to read DataTypeDefinition\n");
        }
        UA_ReadRequest_clear(&req2);
        UA_ReadResponse_clear(&resp2);
    } else {
        printf("Failed to read DataType attribute\n");
    }
    UA_ReadRequest_clear(&req);
    UA_ReadResponse_clear(&resp);
}

void mirua_print_node(MiruaContext* ctx, const UA_NodeId* node, int indent) {
    char* buf = dbg_getbuf(0);
    size_t bufsize = dbg_buf_size();
    _mirua_print_node(buf, bufsize, ctx->client, ctx->config.printLevel, node, indent);
}

void _mirua_print_node(
    char* buf,
    size_t bufsize,
    UA_Client* client,
    int printLevel,
    const UA_NodeId* node,
    int indent) {
    for (int i = 0; i < indent; ++i) printf("  ");
    switch (printLevel) {
        case 0:
            mirua_to_string_nodeId(buf, bufsize, client, node);
            printf("%s\n", buf);
            break;
        case 1:
            mirua_to_string_nodeIdEx(buf, bufsize, client, node);
            printf("%s\n", buf);
            break;
        case 2:
            mirua_to_string_nodeId(buf, bufsize, client, node);
            printf("%s\n", buf);
            memset(buf, 0, bufsize);
            mirua_to_string_nodeId_datatype(buf, bufsize, client, node);
            printf("%s\n", buf);
            break;
    }
}
void mirua_print_current_children(MiruaContext* ctx) {
    for (size_t i = 0; i < ctx->currentChildren.size; i++) {
        printf("[%zu]", i);
        mirua_print_node(ctx, &ctx->currentChildren.nodeIds[i].nodeid, 1);
    }
}

void mirua_print_current_node(MiruaContext* ctx) {
    printf("[CURRENT NODE] - ");
    mirua_print_node(ctx, &ctx->currentNode.nodeid, 0);
}

void mirua_print_node_path(UA_Client* client, const UA_NodeId* nodeId) {
    UA_NodeId currentId;
    UA_NodeId_copy(nodeId, &currentId);

    //
    char path[1024] = "";
    size_t path_len = 0;

    while (!UA_NodeId_isNull(&currentId)) {
        UA_BrowseRequest bReq;
        UA_BrowseRequest_init(&bReq);
        bReq.nodesToBrowseSize = 1;
        bReq.nodesToBrowse = UA_Array_new(1, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]);
        UA_BrowseDescription_init(&bReq.nodesToBrowse[0]);
        UA_NodeId_copy(&currentId, &bReq.nodesToBrowse[0].nodeId);
        bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_INVERSE;
        bReq.nodesToBrowse[0].referenceTypeId =
            UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
        bReq.nodesToBrowse[0].includeSubtypes = true;
        bReq.nodesToBrowse[0].nodeClassMask = 0;
        bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_BROWSENAME;

        UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);

        if (bResp.resultsSize == 1 && bResp.results[0].referencesSize > 0) {
            // Get parent node
            UA_ReferenceDescription* ref = &bResp.results[0].references[0];
            // Prepend BrowseName to path
            char segment[256];
            snprintf(
                segment,
                sizeof(segment),
                "/%.*s",
                (int)ref->browseName.name.length,
                ref->browseName.name.data);
            size_t seg_len = strlen(segment);
            memmove(path + seg_len, path, path_len + 1);
            memcpy(path, segment, seg_len);
            path_len += seg_len;
            // Move up to parent
            UA_NodeId parentId;
            UA_NodeId_init(&parentId);
            UA_NodeId_copy(&ref->nodeId.nodeId, &parentId);
            UA_NodeId_clear(&currentId);
            UA_NodeId_copy(&parentId, &currentId);
            UA_NodeId_clear(&parentId);
        } else {
            // No parent, reached root
            break;
        }
        UA_BrowseRequest_clear(&bReq);
        UA_BrowseResponse_clear(&bResp);
    }
    printf("Node path: %s\n", path[0] ? path : "/");
    UA_NodeId_clear(&currentId);
}

void mirua_print_node_value_json(UA_Client* client, const UA_NodeId* nodeId) {
    UA_Variant value;
    UA_Variant_init(&value);

    char* buf = dbg_getbuf(0);
    size_t bufsize = dbg_buf_size();
    mirua_to_string_nodeId(buf, bufsize, client, nodeId);

    UA_StatusCode status = UA_Client_readValueAttribute(client, *nodeId, &value);
    if (status == UA_STATUSCODE_GOOD) {
        UA_EncodeJsonOptions options;
        memset(&options, 0, sizeof(options));
        options.prettyPrint = true;
        options.unquotedKeys = false;
        options.useReversible = true;
        options.stringNodeIds = true;

        UA_String json;
        UA_String_init(&json);

        UA_encodeJson(value.data, value.type, &json, &options);

        printf("Value of node %s in JSON:\n%.*s\n", buf, (int)json.length, json.data);

        UA_String_clear(&json);
    } else {
        printf("Failed to read value for node %s, status: %s\n", buf, UA_StatusCode_name(status));
    }
    UA_Variant_clear(&value);
}

size_t mirua_to_string_nodeId_datatype(
    char* buf, size_t bufsize, UA_Client* client, const UA_NodeId* node) {
    int size = 1;
    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    request.nodesToReadSize = size;
    request.nodesToRead = UA_Array_new(size, &UA_TYPES[UA_TYPES_READVALUEID]);

    UA_ReadValueId_init(&request.nodesToRead[0]);
    UA_NodeId_copy(node, &request.nodesToRead[0].nodeId);
    request.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DATATYPE;

    UA_ReadResponse response = UA_Client_Service_read(client, request);
    char indent[3];
    memset(indent, ' ', sizeof(indent) - 1);
    indent[sizeof(indent) - 1] = '\0';
    for (size_t i = 0; i < response.resultsSize; ++i) {
        int offset = 0;
        if (response.results[i].hasValue &&
            UA_Variant_hasScalarType(&response.results[i].value, &UA_TYPES[UA_TYPES_NODEID])) {
            UA_NodeId* dataTypeId = (UA_NodeId*)response.results[i].value.data;

            // Print TypeId
            offset = snprintf(
                buf,
                bufsize,
                "%s[DataType] TypeId: ns=%u;i=%u",
                indent,
                dataTypeId->namespaceIndex,
                dataTypeId->identifier.numeric);

            // Read BrowseName (TypeName) of the DataType node
            UA_QualifiedName browseName;
            UA_QualifiedName_init(&browseName);
            UA_StatusCode retval =
                UA_Client_readBrowseNameAttribute(client, *dataTypeId, &browseName);
            if (retval == UA_STATUSCODE_GOOD) {
                offset += snprintf(
                    buf + offset,
                    bufsize - offset,
                    ", TypeName: %.*s",
                    (int)browseName.name.length,
                    browseName.name.data);
            } else {
                offset += snprintf(buf + offset, bufsize - offset, ", TypeName: <read failed>");
            }
            UA_QualifiedName_clear(&browseName);
        } else if (!response.results[i].hasValue) {
            offset += snprintf(buf, bufsize, "%s[Datatype] - {NULL}", indent);
        } else {
            offset += snprintf(
                buf,
                bufsize,
                "%s[Datatype] - {Not implemented for type %s}",
                indent,
                response.results[i].value.type ? response.results[i].value.type->typeName
                                               : "Unknown");
        }
    }

    UA_ReadRequest_clear(&request);
    UA_ReadResponse_clear(&response);
    return 0;
}

int mirua_history_to_string(
    char* buf,
    size_t bufsize,
    UA_Client* client,
    NodeIdHistory* hist,
    mirua_fn_toStringNodeId func) {
    size_t offset = 0;
    char* buf2 = dbg_getbuf(
        1);  // TODO: Hehehhehehe :will make bug later if you use this buf above this :)))
    if (hist->count <= 0) {
        snprintf(buf, bufsize, "History empty!");
        return 0;
    }

    for (size_t i = 0; i < hist->count; i++) {
        MiruaNodeId node = hist->nodeIds[i];
        func(buf2, bufsize, client, &node.nodeid);
        size_t len = strlen(buf2);
        offset += snprintf(buf + offset, bufsize - offset, "%.*s\n", (int)len, buf2);
    }
    // TODO: maybe memset buffers to something after use to find bugs easier?
    return 0;
}

// CONFIG

const MiruaConfigMapping mirua_config_mappings[] = {
    {"endpoint", MIRUA_CONFIG_TYPE_STRING, offsetof(MiruaConfig, endpoint)},
    {"defaultRoot", MIRUA_CONFIG_TYPE_NODEID, offsetof(MiruaConfig, defaultRoot)},
    {"filter", MIRUA_CONFIG_TYPE_FILTER, offsetof(MiruaConfig, selectedDataTypeKinds)},
    {"filter_type", MIRUA_CONFIG_TYPE_FILTER_FUNC, offsetof(MiruaConfig, filterType)},
    {"print_level", MIRUA_CONFIG_TYPE_PRINT_LEVEL, offsetof(MiruaConfig, printLevel)},
};

const size_t mirua_config_mapping_count =
    sizeof(mirua_config_mappings) / sizeof(mirua_config_mappings[0]);

void mirua_config_print(const MiruaConfig* config) {
    for (size_t i = 0; i < mirua_config_mapping_count; ++i) {
        const MiruaConfigMapping* map = &mirua_config_mappings[i];
        printf("[%zu] %s: ", i, map->name);
        mirua_config_print_field(config, map, false);  // Summary mode
        printf("\n");
    }
}


void mirua_config_print_ctx(MiruaContext* ctx) {
    mirua_config_print(&ctx->config);
}
void mirua_config_set_by_idx_ctx(MiruaContext* ctx, size_t idx, const char* value) {
    mirua_config_set_by_idx(&ctx->config, idx, value);
}

void mirua_config_set_by_idx(MiruaConfig* config, size_t idx, const char* value) {
    if (idx >= mirua_config_mapping_count) return;

    const MiruaConfigMapping* map = &mirua_config_mappings[idx];
    switch (map->type) {
        case MIRUA_CONFIG_TYPE_STRING: {
            char** field = (char**)((char*)config + map->offset);
            free(*field);
            *field = strdup(value);
            break;
        }
        case MIRUA_CONFIG_TYPE_NODEID: {
            UA_NodeId* field = (UA_NodeId*)((char*)config + map->offset);
            UA_NodeId_clear(field);

            UA_String str = UA_String_fromChars(value);
            UA_NodeId tempNode;
            UA_NodeId_init(&tempNode);
            UA_StatusCode status = UA_NodeId_parse(&tempNode, str);

            if (status == UA_STATUSCODE_GOOD) {
                UA_NodeId_copy(&tempNode, field);
                log_trace("[CONFIG] Set NodeId field to: %s", value);
            } else {
                log_warn(
                    "[CONFIG] Failed to parse NodeId: %s, status: %s",
                    value,
                    UA_StatusCode_name(status));
            }

            UA_String_clear(&str);
            UA_NodeId_clear(&tempNode);
            break;
        }
        case MIRUA_CONFIG_TYPE_FILTER: {
            uint32_t* field = (uint32_t*)((char*)config + map->offset);
            *field = mirua_parse_filters(value);
            break;
        }
        case MIRUA_CONFIG_TYPE_FILTER_FUNC: {
            MiruaFilterType* field = (MiruaFilterType*)((char*)config + map->offset);
            *field = mirua_filter_parse_type(value);
            break;
        }
        case MIRUA_CONFIG_TYPE_PRINT_LEVEL: {
            int* field = (int*)((char*)config + map->offset);
            *field = atoi(value);
        } break;

        case MIRUA_CONFIG_TYPE_FILE_OUTPUT_PATH: {
            char** field = (char**)((char*)config + map->offset);
            free(*field);
            *field = strdup(value);
        } break;
        default: {
            log_error("[CONFIG] Unknown config type %d for key '%s'", map->type, map->name);
            break;
        }
    }
}
void mirua_config_print_by_idx(MiruaContext* ctx, size_t idx) {
    if (idx >= mirua_config_mapping_count) {
        log_warn("[CONFIG] Index %zu out of range", idx);
        return;
    }
    const MiruaConfigMapping* map = &mirua_config_mappings[idx];
    printf("[%zu] %s: ", idx, map->name);
    mirua_config_print_field(&ctx->config, map, true);
    printf("\n");
}
void mirua_config_print_field(const MiruaConfig* config, const MiruaConfigMapping* map, bool detailed) {
    switch (map->type) {
        case MIRUA_CONFIG_TYPE_STRING: {
            char* field_value = *(char**) ((char*)config + map->offset);
            printf("%s", field_value ? field_value : "(null)");
        } break;
        case MIRUA_CONFIG_TYPE_NODEID: {
            UA_NodeId* field = (UA_NodeId*)((char*)config + map->offset);
            UA_String str;
            UA_String_init(&str);
            UA_NodeId_printEx(field, &str, NULL);
            printf("%.*s\n", (int)str.length, str.data);
            UA_String_clear(&str);
        } break;
        case MIRUA_CONFIG_TYPE_FILTER: {
            uint32_t field_value = *(uint32_t*) ((char*)config + map->offset);
            printf("0x%08X ", field_value);
            if (detailed) {
                printf("Binary: ");
                for (int i = 31; i >= 0; i--) {
                    printf("%d", (field_value >> i) & 1);
                    if (i % 4 == 0 && i != 0) printf(" ");
                }
                printf("\n");
                mirua_config_print_enabled_data_types(field_value);
            } else {
                printf("(use 'ls %zu' for details)", map - mirua_config_mappings);  // Index hint
            }
        } break;
        case MIRUA_CONFIG_TYPE_FILTER_FUNC: {
            MiruaFilterType field_value = *(MiruaFilterType*) ((char*)config + map->offset);
            printf("%s", mirua_filter_get_name(field_value));
        } break;
        case MIRUA_CONFIG_TYPE_PRINT_LEVEL: {
            int field_value = *(int*) ((char*)config + map->offset);
            printf("%d", field_value);
        } break;
        case MIRUA_CONFIG_TYPE_FILE_OUTPUT_PATH: {
            char* field_value = *(char**) ((char*)config + map->offset);
            printf("%s", field_value ? field_value : "(null)");
        } break;
        default:
            printf("Unknown type");
            break;
    }
}


void mirua_config_set(MiruaConfig* config, const char* key, const char* value) {
    for (size_t i = 0; i < mirua_config_mapping_count; ++i) {
        const MiruaConfigMapping* map = &mirua_config_mappings[i];
        if (strcmp(map->name, key) == 0) {
            mirua_config_set_by_idx(config, i, value);
            break;
        }
    }
}

void mirua_config_set_ctx(MiruaContext* ctx, const char* key, const char* value) {
    mirua_config_set(&ctx->config, key, value);
}

static inline bool mirua_isvalid_filter_str(const char* str) {
    if (!str || strlen(str) == 0) return false;

    char* copy = strdup(str);
    char* token = strtok(copy, ",");
    while (token) {
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';

        // Check for range: number-number
        char* dash = strchr(token, '-');
        if (!dash) {
            free(copy);
            return false;
        }
        char *endptr1, *endptr2;
        int start = strtol(token, &endptr1, 10);
        int stop = strtol(dash + 1, &endptr2, 10);
        if (endptr1 != dash || *endptr2 != '\0' || start < 0 || stop < start || stop >= 32) {
            free(copy);
            return false;
        }

        token = strtok(NULL, ",");
    }
    free(copy);
    return true;
}

bool mirua_config_load_from_file(MiruaConfig* config, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        log_warn("[CONFIG] Failed to open config file: %s", filepath);
        return false;
    }

    char line[256];
    int line_num = 0;
    bool has_errors = false;
    uint32_t set_keys = 0;  // Bitmask to track set keys

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;

        char key[64], value[256];
        int parsed = sscanf(line, "%63[^=]=%255s", key, value);

        if (parsed == 2) {
            if (strlen(key) >= 63 || strlen(value) >= 255) {
                log_warn("[CONFIG] Line %d: Key or value too long: %s", line_num, line);
                has_errors = true;
                continue;
            }

            bool valid_key = false;
            size_t key_idx = 0;
            for (size_t i = 0; i < mirua_config_mapping_count; ++i) {
                if (strcmp(key, mirua_config_mappings[i].name) == 0) {
                    valid_key = true;
                    key_idx = i;
                    break;
                }
            }
            if (!valid_key) {
                log_warn("[CONFIG] Line %d: Unknown key '%s'", line_num, key);
                has_errors = true;
                continue;
            }

            if (strcmp(key, "endpoint") == 0) {
                if (strlen(value) == 0 || strstr(value, "opc.tcp://") != value) {
                    log_warn("[CONFIG] Line %d: Invalid endpoint '%s'", line_num, value);
                    has_errors = true;
                    continue;
                }
            } else if (strcmp(key, "defaultRoot") == 0) {
                UA_String str = UA_String_fromChars(value);
                UA_NodeId temp;
                UA_NodeId_init(&temp);
                UA_StatusCode status = UA_NodeId_parse(&temp, str);
                if (status != UA_STATUSCODE_GOOD) {
                    log_warn(
                        "[CONFIG] Line %d: Invalid NodeId '%s', status: %s",
                        line_num,
                        value,
                        UA_StatusCode_name(status));
                    has_errors = true;
                }
                UA_String_clear(&str);
                UA_NodeId_clear(&temp);
                if (status != UA_STATUSCODE_GOOD) continue;
            } else if (strcmp(key, "filter") == 0) {
                if (!mirua_isvalid_filter_str(value)) {
                    log_warn("[CONFIG] Line %d: Invalid filter range '%s'", line_num, value);
                    has_errors = true;
                    continue;
                }
            } else if (strcmp(key, "default_path") == 0) {
                mirua_config_set(config, "output_path", value);
            } else {
                log_warn("[CONFIG] - didnt find val \"%s\" in config", value);
            }

            // If all checks pass, set the config and mark key as set
            mirua_config_set(config, key, value);
            set_keys |= (1U << key_idx);
        } else {
            log_warn("[CONFIG] Line %d: Invalid syntax (expected 'key=value'): %s", line_num, line);
            has_errors = true;
        }
    }

    fclose(f);

    // Check for missing required keys
    uint32_t required_mask = (1U << mirua_config_mapping_count) - 1;  // All bits set
    if (set_keys != required_mask) {
        for (size_t i = 0; i < mirua_config_mapping_count; ++i) {
            if (!(set_keys & (1U << i))) {
                log_warn("[CONFIG] Missing required key: %s", mirua_config_mappings[i].name);
                has_errors = true;
            }
        }
    }

    if (has_errors) {
        log_warn("[CONFIG] Loaded with errors from %s", filepath);
        return false;
    }
    log_trace("[CONFIG] Successfully loaded from %s", filepath);
    return true;
}
void mirua_config_print_enabled_data_types(uint32_t mask) {
    printf("Enabled Data Types (bitmask: 0x%08X):\n", mask);
    if (mask == 0) {
        printf("  All types (no filtering)\n");
        return;
    }

    bool any_enabled = false;
    for (size_t i = 0; i < UA_DATATYPEKINDS; ++i) {  // UA_DATATYPEKINDS = 31
        if (mask & (1U << i)) {
            const char* type_name = (i < UA_DATATYPEKINDS) ? UA_TYPES[i].typeName : "Unknown";
            printf("  [%zu] %s\n", i, type_name);
            any_enabled = true;
        }
    }
    if (!any_enabled) {
        printf("  None (all types disabled)\n");
    }
}
void mirua_print_enabled_data_types(uint32_t mask) {
    printf("Enabled Data Types (bitmask: 0x%08X):\n", mask);
    if (mask == 0) {
        printf("  All types (no filtering)\n");
        return;
    }

    // List of UA_DataTypeKind names (based on open62541 enum, all 31 types)
    const char* type_names[] = {
        "BOOLEAN",           // 0
        "SBYTE",             // 1
        "BYTE",              // 2
        "INT16",             // 3
        "UINT16",            // 4
        "INT32",             // 5
        "UINT32",            // 6
        "INT64",             // 7
        "UINT64",            // 8
        "FLOAT",             // 9
        "DOUBLE",            // 10
        "STRING",            // 11
        "DATETIME",          // 12
        "GUID",              // 13
        "BYTESTRING",        // 14
        "XMLELEMENT",        // 15
        "NODEID",            // 16
        "EXPANDEDNODEID",    // 17
        "STATUSCODE",        // 18
        "QUALIFIEDNAME",     // 19
        "LOCALIZEDTEXT",     // 20
        "EXTENSIONOBJECT",   // 21
        "DATAVALUE",         // 22
        "VARIANT",           // 23
        "DIAGNOSTICINFO",    // 24
        "DECIMAL",           // 25
        "ENUM",              // 26
        "STRUCTURE",         // 27
        "OPTSTRUCT",         // 28
        "UNION",             // 29
        "BITFIELDCLUSTER"    // 30
    };
    const size_t num_types = sizeof(type_names) / sizeof(type_names[0]);

    bool any_enabled = false;
    for (size_t i = 0; i < num_types && i < 32; ++i) {
        if (mask & (1U << i)) {
            printf("  [%zu] %s\n", i, type_names[i]);
            any_enabled = true;
        }
    }
    if (!any_enabled) {
        printf("  None (all types disabled)\n");
    }
}
uint32_t mirua_parse_filters(const char* filters_str) {
    uint32_t mask = 0;
    if (!filters_str || strlen(filters_str) == 0) return mask;

    char* copy = strdup(filters_str);
    char* token = strtok(copy, ",");
    while (token) {
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';

        // Check for range (e.g., "1-10")
        char* dash = strchr(token, '-');
        if (dash) {
            int start = atoi(token);
            int stop = atoi(dash + 1);
            if (start < 0) {
                log_error("[FILTER] Start range %d < 0, clamping to 0", start);
                start = 0;
            }
            if (stop > 32) {
                log_error("[FILTER] Stop range %d > 32, clamping to 32", stop);
                stop = 32;
            }
            if (stop < start) {
                log_error("[FILTER] Stop range %d < start %d, clamping to %d", stop, start, start);
                stop = start;
            }
            for (int i = start; i <= stop && i < 32; ++i) {
                mask |= (1U << i);
            }
        } else {
            log_error("[FILTER] Invalid token '%s', expected range like '0-31'", token);
        }

        token = strtok(NULL, ",");
    }
    free(copy);
    return mask;
}

void mirua_explore_value(MiruaContext* ctx, const UA_NodeId* nodeId) {
    (void)nodeId;
    //  ns=5;s=::Program1:mouse_advv
    //
    //
    // if (!nodeId) {
    //     log_error("[EXPLORE] Invalid nodeId");
    //     return;
    // }

    // // List of attributes to read (expand as needed)
    // const UA_UInt32 attributes[] = {
    //     UA_ATTRIBUTEID_NODEID,
    //     UA_ATTRIBUTEID_NODECLASS,
    //     UA_ATTRIBUTEID_BROWSENAME,
    //     UA_ATTRIBUTEID_DISPLAYNAME,
    //     UA_ATTRIBUTEID_DESCRIPTION,
    //     UA_ATTRIBUTEID_VALUE,
    //     UA_ATTRIBUTEID_DATATYPE,
    //     UA_ATTRIBUTEID_VALUERANK,
    //     UA_ATTRIBUTEID_ACCESSLEVEL,
    //     UA_ATTRIBUTEID_USERACCESSLEVEL,
    //     UA_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL,
    //     UA_ATTRIBUTEID_HISTORIZING,
    //     UA_ATTRIBUTEID_EXECUTABLE,
    //     UA_ATTRIBUTEID_USEREXECUTABLE,
    //     UA_ATTRIBUTEID_DATATYPEDEFINITION,
    //     UA_ATTRIBUTEID_ROLEPERMISSIONS,
    //     UA_ATTRIBUTEID_ACCESSRESTRICTIONS};
    // const size_t numAttributes = sizeof(attributes) / sizeof(attributes[0]);

    // // Prepare read request
    // UA_ReadRequest request;
    // UA_ReadRequest_init(&request);
    // request.nodesToReadSize = numAttributes;
    // request.nodesToRead = UA_Array_new(numAttributes, &UA_TYPES[UA_TYPES_READVALUEID]);

    // for (size_t i = 0; i < numAttributes; ++i) {
    //     UA_ReadValueId_init(&request.nodesToRead[i]);
    //     UA_NodeId_copy(nodeId, &request.nodesToRead[i].nodeId);
    //     request.nodesToRead[i].attributeId = attributes[i];
    // }

    // // Perform read
    // UA_ReadResponse response = UA_Client_Service_read(ctx->client, request);

    // // Print results
    // printf("Detailed info for node: ");
    // char buf[256];
    // mirua_to_string_nodeId(buf, sizeof(buf), ctx->client, nodeId);
    // printf("%s\n", buf);

    // UA_EncodeJsonOptions options;
    // memset(&options, 0, sizeof(options));
    // options.prettyPrint = true;
    // options.unquotedKeys = false;
    // options.useReversible = true;
    // options.stringNodeIds = true;

    // for (size_t i = 0; i < response.resultsSize; ++i) {
    //     const char* attrName = mirua_to_string_attribute_id(attributes[i]);
    //     printf("Attribute: %s\n", attrName);

    //     if (response.results[i].hasValue) {
    //         UA_String json;
    //         UA_String_init(&json);

    //         // Handle ExtensionObjects specially (e.g., DataTypeDefinition)
    //         if (response.results[i].value.type == &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]) {
    //             UA_ExtensionObject* eo = (UA_ExtensionObject*)response.results[i].value.data;
    //             if (eo->encoding == UA_EXTENSIONOBJECT_DECODED && eo->content.decoded.type) {
    //                 UA_encodeJson(
    //                     eo->content.decoded.data, eo->content.decoded.type, &json, &options);
    //             } else {
    //                 // UA_String_FromChars(&json, "<not decoded or unknown type>");
    //             }
    //         } else {
    //             UA_encodeJson(
    //                 response.results[i].value.data,
    //                 response.results[i].value.type,
    //                 &json,
    //                 &options);
    //         }

    //         printf("%.*s\n", (int)json.length, json.data);
    //         UA_String_clear(&json);
    //     } else {
    //         printf("  <no value>\n");
    //     }
    //     printf("\n");
    // }

    // // Cleanup

    // UA_ReadRequest_clear(&request);
    // UA_ReadResponse_clear(&response);
    UA_String str = UA_String_fromChars("ns=5;s=::Program1:mouse_advv");
    UA_NodeId node;
    // UA_StatusCode status =
    UA_NodeId_parse(&node, str);

    // VAL AND TYPE of VAL
    UA_Variant var;
    UA_Variant_init(&var);
    UA_Client_readValueAttribute(ctx->client, node, &var);
    UA_QualifiedName name;
    UA_QualifiedName_init(&name);
    UA_Client_readBrowseNameAttribute(ctx->client, var.type->typeId, &name);
    UA_NodeId out;
    UA_NodeId_init(&out);
    UA_Client_readDataTypeAttribute(ctx->client, node, &out);

    // NODE STUFF
    UA_QualifiedName name2;
    UA_QualifiedName_init(&name2);
    UA_Client_readBrowseNameAttribute(ctx->client, node, &name2);
    UA_QualifiedName nam;
    UA_QualifiedName_init(&nam);
    UA_Client_readBrowseNameAttribute(ctx->client, out, &nam);

    MiruaValue val;
    UA_NodeId_init(&val.type.nodeid);
    UA_Variant_init(&val.value);
    UA_QualifiedName_init(&val.type.name);
    UA_NodeId_copy(&out, &val.type.nodeid);  // nodeid of the "parent" nodetype
    UA_Variant_copy(&var, &val.value);
    UA_QualifiedName_copy(&nam, &val.type.name);  // "sturcture" type of the value

    MiruaNodeId mirNodeId;
    UA_NodeId_init(&mirNodeId.nodeid);
    UA_QualifiedName_init(&mirNodeId.name);
    UA_NodeId_copy(&node, &mirNodeId.nodeid);        // Copy the parsed nodeId
    UA_QualifiedName_copy(&name2, &mirNodeId.name);  // Copy the browse name

    MiruaTreeNode* root = mirua_tree_node_create(&mirNodeId, &val, &out);
    MiruaTree* tree = mirua_tree_create(root);

    size_t bufsize = 20000;
    char buf[bufsize];
    size_t offset = 0;
    // offset += mirua_tree_node_to_string(buf, 1024, root, 0);
    // offset += mirua_nodeId_to_string(buf, 1024, &root->nodeid);
    // offset += mirua_value_to_string(buf + offset, 1024 - offset, &root->value);
    // printf("%s", buf);

    if (root) {
        mirua_build_structure_tree(ctx, tree, root, &node);
    }

    offset += mirua_tree_to_string(buf, bufsize, tree);
    printf("%s\n", buf);
    //  UA_NS0ID_HASCOMPONENT from browse result to check if has HasComponent
    //  if node is extensionObject check for its node datatype. take name.
    //  Then make root of the node.
    //  Check node's references. does it have any HasComponent -> if yes repeat. and check that node
    //  with the same steps as above ^ Should have "sturct" built. it will stop at built in types
    //  such as int or string etc.

    mirua_t_NodeList references;
    mirua_init_nodeList(&references, 128);
    mirua_explore_children(ctx, &references, &node);  // Browse references of the mouse_advv node

    printf("Browsing references for node ns=5;s=::Program1:mouse_advv:\n");
    for (size_t i = 0; i < references.size; i++) {
        printf("[%zu] ", i);
        mirua_print_node(ctx, &references.nodeIds[i].nodeid, 0);
    }

    // Create the tree node with the MiruaNodeId

    UA_EncodeJsonOptions options;
    memset(&options, 0, sizeof(options));
    options.prettyPrint = true;
    options.unquotedKeys = false;
    options.useReversible = true;
    options.stringNodeIds = true;

    UA_String json;
    UA_String_init(&json);

    if (var.type == &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]) {
        UA_ExtensionObject* eo = (UA_ExtensionObject*)var.data;
        if (eo->encoding == UA_EXTENSIONOBJECT_DECODED && eo->content.decoded.type) {
            // Already decoded
            UA_encodeJson(eo->content.decoded.data, eo->content.decoded.type, &json, &options);
        }
    } else {
        UA_encodeJson(var.data, var.type, &json, &options);
    }

    UA_String_clear(&str);
    UA_NodeId_clear(&node);
}

MiruaTree* mirua_tree_create(MiruaTreeNode* root) {
    if (!root) {
        log_warn("[TREE] - Tried to create tree with invalid root node.");
    }

    MiruaTree* tree = malloc(sizeof(MiruaTree));
    if (!tree) {
        log_error("[TREE] - Failed to malloc sizeof tree");
    }

    tree->root = root;
    tree->total_nodes = 1;
    log_trace("[TREE] - Created MIRUA_TREE:");
    return tree;
}
void mirua_tree_destroy(MiruaTree* tree) {
    if (!tree) return;
    mirua_tree_node_destroy(tree->root);
    free(tree);
    log_trace("[TREE] Destroyed MiruaTree");
}

void mirua_tree_node_destroy(MiruaTreeNode* node) {
    if (!node) {
        log_warn("[TREE_NODE] - Tried to destroy empty node.");
    }

    for (size_t i = 0; i < node->child_count; i++) {
        mirua_tree_node_destroy(node->children[i]);
    }
    // NODEID
    // TODO: make proper miruaNodeId_create etc?
    UA_NodeId_clear(&node->nodeid.nodeid);
    UA_QualifiedName_clear(&node->nodeid.name);
    // VALUE
    mirua_value_clear(&node->value);
    // TYPE
    UA_NodeId_clear(&node->type);

    free(node->children);
    free(node);
    log_trace("[TREE NODE] - Destroyed.");
}

void mirua_tree_add_child(MiruaTree* tree, MiruaTreeNode* parent, MiruaTreeNode* child) {
    if (!tree || !parent || !child) {
        log_warn("[TREE] Invalid parameters for add_child");
        return;
    }

    if (parent->child_count >= parent->child_capacity) {
        size_t new_cap = parent->child_capacity * 2;
        MiruaTreeNode** new_children = realloc(parent->children, sizeof(MiruaTreeNode*) * new_cap);
        if (!new_children) {
            log_error("[TREE] Failed to resize children array");
            return;
        }
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }

    parent->children[parent->child_count++] = child;
    tree->total_nodes++;
    log_trace("[TREE] Added child, total nodes: %zu", tree->total_nodes);
}

static size_t mirua_tree_node_count_nodes(const MiruaTreeNode* node) {
    if (!node) return 0;
    size_t count = 1;
    for (size_t i = 0; i < node->child_count; ++i) {
        count += mirua_tree_node_count_nodes(node->children[i]);
    }
    return count;
}

size_t mirua_tree_count_nodes(const MiruaTree* tree) {
    if (!tree || !tree->root) return 0;
    return mirua_tree_node_count_nodes(tree->root);
}

MiruaTreeNode* mirua_tree_node_create(
    const MiruaNodeId* nodeId, const MiruaValue* value, const UA_NodeId* type) {
    MiruaTreeNode* node = malloc(sizeof(MiruaTreeNode));
    if (!node) {
        log_error("[TREE] Failed to allocate MiruaTreeNode");
        return NULL;
    }

    memset(node, 0, sizeof(MiruaTreeNode));

    mirua_nodeId_init(&node->nodeid);
    mirua_value_init(&node->value);
    UA_NodeId_init(&node->type);
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;

    if (nodeId) {
        UA_NodeId_copy(&nodeId->nodeid, &node->nodeid.nodeid);
        UA_QualifiedName_copy(&nodeId->name, &node->nodeid.name);
    }
    if (value) {
        UA_Variant_copy(&value->value, &node->value.value);
        UA_NodeId_copy(&value->type.nodeid, &node->value.type.nodeid);
        UA_QualifiedName_copy(&value->type.name, &node->value.type.name);

        // Determine node type based on value
        if (value->value.type == &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]) {
            node->nodeType = MIRUA_NODE_STRUCTURE;
        } else {
            node->nodeType = MIRUA_NODE_VALUE;
        }
    }
    if (type) {
        UA_NodeId_copy(type, &node->type);
    }

    node->child_capacity = 4;
    node->children = malloc(sizeof(MiruaTreeNode*) * node->child_capacity);
    if (!node->children) {
        log_error("[TREE] Failed to allocate children array");

        mirua_nodeId_clear(&node->nodeid);
        mirua_value_clear(&node->value);
        UA_NodeId_clear(&node->type);
        free(node);
        return NULL;
    }
    log_trace("[TREE] Created MiruaTreeNode");
    return node;
}

void mirua_nodeId_init(MiruaNodeId* nodeId) {
    if (!nodeId) return;
    UA_NodeId_init(&nodeId->nodeid);
    UA_QualifiedName_init(&nodeId->name);
    nodeId->dataTypeKind = UA_DATATYPEKIND_BITFIELDCLUSTER;  
}

void mirua_nodeId_clear(MiruaNodeId* nodeId) {
    if (!nodeId) return;
    UA_NodeId_clear(&nodeId->nodeid);
    UA_QualifiedName_clear(&nodeId->name);
}

MiruaNodeId* mirua_nodeId_create(const UA_NodeId* nodeId, const UA_QualifiedName* name) {
    MiruaNodeId* newNodeId = malloc(sizeof(MiruaNodeId));
    if (!newNodeId) {
        log_error("[NODEID] Failed to allocate MiruaNodeId");
        return NULL;
    }
    mirua_nodeId_init(newNodeId);

    if (nodeId) UA_NodeId_copy(nodeId, &newNodeId->nodeid);
    if (name) UA_QualifiedName_copy(name, &newNodeId->name);

    log_trace("[NODEID] Created MiruaNodeId");
    return newNodeId;
}

void mirua_nodeId_destroy(MiruaNodeId* nodeId) {
    if (!nodeId) return;
    mirua_nodeId_clear(nodeId);
    free(nodeId);
    log_trace("[NODEID] Destroyed MiruaNodeId");
}

// MiruaValue functions
void mirua_value_init(MiruaValue* value) {
    if (!value) return;
    UA_Variant_init(&value->value);
    mirua_nodeId_init(&value->type);
}

void mirua_value_clear(MiruaValue* value) {
    if (!value) return;
    UA_Variant_clear(&value->value);
    mirua_nodeId_clear(&value->type);
}

MiruaValue* mirua_value_create(const UA_Variant* value, const MiruaNodeId* type) {
    MiruaValue* newValue = malloc(sizeof(MiruaValue));
    if (!newValue) {
        log_error("[VALUE] Failed to allocate MiruaValue");
        return NULL;
    }
    mirua_value_init(newValue);

    if (value) UA_Variant_copy(value, &newValue->value);
    if (type) {
        UA_NodeId_copy(&type->nodeid, &newValue->type.nodeid);
        UA_QualifiedName_copy(&type->name, &newValue->type.name);
    }

    log_trace("[VALUE] Created MiruaValue");
    return newValue;
}

void mirua_value_destroy(MiruaValue* value) {
    if (!value) return;
    mirua_value_clear(value);
    free(value);
    log_trace("[VALUE] Destroyed MiruaValue");
}

// Recursive function to build the structure tree by exploring HasComponent references
void mirua_build_structure_tree(
    MiruaContext* ctx, MiruaTree* tree, MiruaTreeNode* parent, const UA_NodeId* dataTypeId) {
    // Browse HasComponent references of the data type node
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    UA_BrowseDescription_init(&bReq.nodesToBrowse[0]);
    UA_NodeId_copy(dataTypeId, &bReq.nodesToBrowse[0].nodeId);
    bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bReq.nodesToBrowse[0].referenceTypeId =
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);  // HasComponent
    bReq.nodesToBrowse[0].includeSubtypes = true;
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

    UA_BrowseResponse bResp = UA_Client_Service_browse(ctx->client, bReq);

    if (bResp.resultsSize > 0 && bResp.results[0].referencesSize > 0) {
        for (size_t i = 0; i < bResp.results[0].referencesSize; ++i) {
            UA_ReferenceDescription* ref = &bResp.results[0].references[i];

            // Create a child node for this field
            MiruaNodeId fieldNodeId;
            mirua_nodeId_init(&fieldNodeId);
            UA_NodeId_copy(&ref->nodeId.nodeId, &fieldNodeId.nodeid);
            UA_QualifiedName_copy(&ref->browseName, &fieldNodeId.name);

            // Read the field's data type
            UA_NodeId fieldDataType;
            UA_NodeId_init(&fieldDataType);
            UA_Client_readDataTypeAttribute(ctx->client, ref->nodeId.nodeId, &fieldDataType);

            // Read the field's value to check its data type
            UA_Variant fieldValueVariant;
            UA_Variant_init(&fieldValueVariant);
            UA_StatusCode valueStatus =
                UA_Client_readValueAttribute(ctx->client, ref->nodeId.nodeId, &fieldValueVariant);

            // Create a MiruaValue for the field
            MiruaValue fieldValue;
            mirua_value_init(&fieldValue);
            UA_NodeId_copy(&fieldDataType, &fieldValue.type.nodeid);  // Copy data type NodeId
            if (valueStatus == UA_STATUSCODE_GOOD) {
                UA_Variant_copy(&fieldValueVariant, &fieldValue.value);
            }

            MiruaTreeNode* child =
                mirua_tree_node_create(&fieldNodeId, &fieldValue, &fieldDataType);
            if (child) {
                mirua_tree_add_child(tree, parent, child);

                // If the field's data type is another structure, recurse
                UA_QualifiedName fieldTypeName;
                UA_QualifiedName_init(&fieldTypeName);
                UA_Client_readBrowseNameAttribute(ctx->client, ref->nodeId.nodeId, &fieldTypeName);

                UA_QualifiedName fieldTypeName2;
                UA_QualifiedName_init(&fieldTypeName2);
                UA_Client_readBrowseNameAttribute(
                    ctx->client, fieldValueVariant.type->typeId, &fieldTypeName2);

                const char* target = "Structure";
                size_t targetLen = strlen(target);
                if (fieldTypeName2.name.length == targetLen &&
                    memcmp(fieldTypeName2.name.data, target, targetLen) == 0) {
                    mirua_build_structure_tree(ctx, tree, child, &ref->nodeId.nodeId);
                }
                UA_QualifiedName_clear(&fieldTypeName);
            }

            mirua_nodeId_clear(&fieldNodeId);
            mirua_value_clear(&fieldValue);
            UA_NodeId_clear(&fieldDataType);
        }
    }

    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);
}

size_t mirua_nodeId_to_string(char* buf, size_t bufsize, const MiruaNodeId* nodeid, size_t indent) {
    if (!buf || bufsize == 0 || !nodeid) return 0;
    size_t written = 0;

    // indent
    for (size_t i = 0; i < indent; ++i) {
        written += snprintf(buf + written, bufsize - written, "  ");
    }

    UA_String str;
    UA_String_init(&str);
    UA_NodeId_print(&nodeid->nodeid, &str);

    UA_String name = nodeid->name.name;
    written = snprintf(
        buf + written,
        bufsize - written,
        "nodeId: %.*s name: %.*s typekind: %s",
        (int)str.length,
        str.data,
        (int)name.length,
        name.data,
        UA_TYPES[nodeid->dataTypeKind].typeName);

    UA_String_clear(&str);
    return written;
}

void mirua_nodeId_copy(const MiruaNodeId* src, MiruaNodeId* dst) {
    if (!src || !dst) return;
    UA_NodeId_copy(&src->nodeid, &dst->nodeid);
    UA_QualifiedName_copy(&src->name, &dst->name);
    dst->dataTypeKind = src->dataTypeKind;  
}

size_t mirua_value_to_string(char* buf, size_t bufsize, const MiruaValue* value, size_t indent) {
    if (!buf || bufsize == 0 || !value) return 0;
    size_t written = 0;

    // indent
    for (size_t i = 0; i < indent; ++i) {
        written += snprintf(buf + written, bufsize - written, "  ");
    }

    UA_String str;
    UA_String_init(&str);
    UA_EncodeJsonOptions options;
    memset(&options, 0, sizeof(options));
    options.prettyPrint = false;   // Human-readable formatting
    options.unquotedKeys = false;  // Standard JSON keys
    options.useReversible = true;  // For round-trip encoding/decoding
    options.stringNodeIds = true;  // NodeIds as strings

    UA_StatusCode status = UA_encodeJson(value->value.data, value->value.type, &str, &options);
    if (status != UA_STATUSCODE_GOOD) {
        printf("Failed to encode Variant to JSON: %s\n", UA_StatusCode_name(status));
    }

    written += snprintf(buf + written, bufsize - written, "[VALUE:VALUE] - ");
    written += snprintf(buf + written, bufsize - written, "%.*s", (int)str.length, str.data);

    written += snprintf(buf + written, bufsize - written, "[VALUE:TYPE] - ");
    written += mirua_nodeId_to_string(buf + written, bufsize - written, &value->type, indent);

    UA_String_clear(&str);

    if (written >= bufsize) {
        assert(0 && "Buffer truncation in mirua_value_to_string - increase bufsize");
    }

    return written;
}

size_t mirua_tree_node_to_string(
    char* buf, size_t bufsize, const MiruaTreeNode* node, size_t indent) {
    if (!buf || bufsize == 0 || !node) return 0;
    size_t written = 0;

    written += snprintf(buf + written, bufsize - written, "\n");

    // indent
    for (size_t i = 0; i < indent; ++i) {
        written += snprintf(buf + written, bufsize - written, "  ");
    }

    // main string build
    written += snprintf(buf + written, bufsize - written, "[TREE_NODE:NODEID] - ");
    written += mirua_nodeId_to_string(buf + written, bufsize - written, &node->nodeid, 0);
    written += mirua_value_to_string(buf + written, bufsize - written, &node->value, 0);
    written += snprintf(
        buf + written,
        bufsize - written,
        "[TREE_NODE:TYPE] %s",
        mirua_node_type_to_string(node->nodeType));
    // TODO: add type?

    if (written >= bufsize) {
        assert(0 && "Buffer truncation in mirua_treenode_to_string - increase bufsize");
    }

    // recursive print for children
    for (size_t i = 0; i < node->child_count; i++) {
        written += mirua_tree_node_to_string(
            buf + written, bufsize - written, node->children[i], 1 + indent);
    }

    return written;
}

size_t mirua_tree_to_string(char* buf, size_t bufsize, const MiruaTree* tree) {
    if (!buf || bufsize == 0 || !tree) return 0;
    size_t written = 0;

    written += mirua_tree_node_to_string(buf, bufsize, tree->root, 0);
    return written;
}

const char* mirua_node_type_to_string(MiruaNodeType type) {
    switch (type) {
        case MIRUA_NODE_STRUCTURE: return "structure";
        case MIRUA_NODE_VALUE: return "value";
    }
}

