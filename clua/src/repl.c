#include "repl.h"

#include <assert.h>

#include "../database/core/fileio.h"
#include "../database/core/nob.h"
#include "log.h"
#include "mirua_serialization.h"
#include "node_pool.h"
// TELEGRAF2
static size_t telegraf_serialize_node(
    memory_arena* arena, String_Builder* sb, const UaNodeIdExpanded* node) {
    sb_appendf_arena(arena, sb, "[[inputs.opcua.nodes]]\n");
    // if (safe_snprintf(buf, &offset, bufsize, "[[inputs.opcua.nodes]]\n") != 0) return SIZE_MAX;

    // Derive unique name from string identifier path (e.g. "::AsGlobalPV:OK10_IO.Din.Spare1" ->
    // "OK10_IO_Din_Spare1")
    if (node->nodeid.identifierType == UA_NODEIDTYPE_STRING) {
        const char* src = (const char*)node->nodeid.identifier.string.data;
        size_t len = node->nodeid.identifier.string.length;

        // Find last ':' to skip namespace prefix like "::AsGlobalPV:"
        const char* last_colon = NULL;
        for (size_t i = len; i > 0; i--) {
            if (src[i - 1] == ':') {
                last_colon = src + i - 1;
                break;
            }
        }
        if (last_colon) {
            len -= (size_t)(last_colon - src) + 1;
            src = last_colon + 1;
        }

        // Replace '.' with '_' into a temp buffer
        char name_buf[256];
        size_t name_len = len < sizeof(name_buf) - 1 ? len : sizeof(name_buf) - 1;
        memcpy(name_buf, src, name_len);
        for (size_t i = 0; i < name_len; i++)
            if (name_buf[i] == '.') name_buf[i] = '_';
        name_buf[name_len] = '\0';

        // if (safe_snprintf(buf, &offset, bufsize, "name = \"%s\"\n", name_buf) != 0) return
        // SIZE_MAX;
        sb_appendf_arena(arena, sb, "name = \"%s\"\n", name_buf);
    } else {
        // Fallback to browse name for numeric/GUID identifiers
        sb_appendf_arena(
            arena,
            sb,
            "name = \"%.*s\"\n",
            (int)node->browseName.name.length,
            node->browseName.name.data);

        // if (safe_snprintf(
        //         buf,
        //         &offset,
        //         bufsize,
        //         "name = \"%.*s\"\n",
        //         (int)node->name.name.length,
        //         node->name.name.data) != 0)
        //     return SIZE_MAX;
    }

    // if (safe_snprintf(buf, &offset, bufsize, "namespace = \"%u\"\n", node->nodeid.namespaceIndex)
    // !=
    //     0)
    //     return SIZE_MAX;
    sb_appendf_arena(arena, sb, "namespace = \"%u\"\n", node->nodeid.namespaceIndex);

    if (node->nodeid.identifierType == UA_NODEIDTYPE_STRING) {
        // if (safe_snprintf(buf, &offset, bufsize, "identifier_type = \"%s\"\n", "s") != 0)
        //     return SIZE_MAX;
        sb_appendf_arena(arena, sb, "identifier_type = \"%s\"\n", "s");
        sb_appendf_arena(
            arena,
            sb,
            "identifier = \"%.*s\"\n",
            (int)node->nodeid.identifier.string.length,
            node->nodeid.identifier.string.data);

        // if (safe_snprintf(
        //         buf,
        //         &offset,
        //         bufsize,
        //         "identifier = \"%.*s\"\n",
        //         (int)node->nodeid.identifier.string.length,
        //         node->nodeid.identifier.string.data) != 0)
        //     return SIZE_MAX;
    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_NUMERIC) {
        // if (safe_snprintf(buf, &offset, bufsize, "identifier_type = \"%s\"\n", "i") != 0)
        //     return SIZE_MAX;
        sb_appendf_arena(arena, sb, "identifier_type = \"%s\"\n", "i");
        sb_appendf_arena(arena, sb, "identifier = \"%u\"\n", node->nodeid.identifier.numeric);

        // if (safe_snprintf(
        //         buf, &offset, bufsize, "identifier = \"%u\"\n", node->nodeid.identifier.numeric)
        //         !=
        //     0)
        //     return SIZE_MAX;

    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_BYTESTRING) {
        assert(0 && "not implemented");
        return SIZE_MAX;
    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_GUID) {
        assert(0 && "not implemented");
        return SIZE_MAX;
    }

    return 0;
}

static size_t telegraf_serialize_nodes(
    memory_arena* arena, String_Builder* sb, NodePool* pool, arr_NodeRef* refs) {
    // if (safe_snprintf(buf, &offset, bufsize, "#GENERATED NODES\n") != 0) return SIZE_MAX;
    sb_appendf_arena(arena, sb, "#GENERATED NODES\n");

    for (size_t i = 0; i < refs->count; i++) {
        UaNodeIdExpanded* node = node_pool_get(pool, refs->items[i]);
        size_t node_written = telegraf_serialize_node(arena, sb, node);
        // if (node_written == SIZE_MAX) return SIZE_MAX;
        // if (safe_snprintf(buf, &offset, bufsize, "\n") != 0) return SIZE_MAX;
        sb_appendf_arena(arena, sb, "\n");
    }

    return 0;
}
static size_t telegraf_serialize_nodes_to_file(
    memory_arena* arena, NodePool* nodes, arr_NodeRef* refs, FILE* file) {
    // size_t bufsize = SERIALIZER_TO_FILE_INITIAL_MEMORY;
    // char* buf = malloc(bufsize);
    // if (!buf) return SIZE_MAX;

    // size_t written;
    String_Builder sb = {0};
    telegraf_serialize_nodes(arena, &sb, nodes, refs);
    // while ((written = telegraf_serialize_nodes(buf, bufsize, nodes)) == SIZE_MAX) {
    //     size_t new_bufsize = bufsize * 2;
    //     char* new_buf = realloc(buf, new_bufsize);
    //     if (!new_buf) {
    //         free(buf);
    //         return SIZE_MAX;
    //     }
    //     buf = new_buf;
    //     bufsize = new_bufsize;
    // }

    // if (fwrite(buf, 1, written, file) != written) {
    //     free(buf);
    //     return SIZE_MAX;
    // }
    fwrite(sb.items, 1, sb.count, file);

    // free(buf);
    return 0;
}
void explore_children(
    memory_arena* strArena, UA_Client* client, NodePool* nodes, NodeRef targetRef) {
    UaNodeIdExpanded* root = node_pool_get(nodes, targetRef);
    // TODO: might need handling incase nil root?

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
            NodeRef childRef = node_pool_add(nodes, child);
            node_pool_add_child(nodes, childRef, targetRef);
        }
    } else {
        log_warn("[EXPLORE] Browse failed or no results for node");
    }

    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);
}

void cmd_ls(void* userdata, const char* args) {
    context* ctx = (context*)userdata;
    String_Builder sb = {0};
    arena_reset(ctx->tempArena, false);
    sb_appendf_arena(ctx->tempArena, &sb, "%*s[0]{%u} ", (int)(0 * 2), "", ctx->current.idx);
    UA_NodeId_Expanded_toString(ctx->tempArena, &sb, node_pool_get(&ctx->nodes, ctx->current));
    sb_appendf_arena(ctx->tempArena, &sb, "\n");

    visit_direct_children(ctx->tempArena, &sb, &ctx->nodes, ctx->current, visit_print, NULL);
    String_View sv = sb_to_sv(sb);
    printf(SV_Fmt "\n", SV_Arg(sv));
}

void cmd_cd_up(void* userdata, const char* args) {
    context* ctx = (context*)userdata;
    String_Builder sb = {0};
    arena_reset(ctx->tempArena, false);

    UaNodeIdExpanded* currentNode = node_pool_get(&ctx->nodes, ctx->current);
    if (node_cmp(currentNode->parentRef, NODE_NILL)) {
        log_warn("No parent to cd.. back to");
        return;
    }

    ctx->current = currentNode->parentRef;
    if (currentNode->explored) {
        visit_direct_children(ctx->tempArena, &sb, &ctx->nodes, ctx->current, visit_print, NULL);
        String_View sv = sb_to_sv(sb);
        printf(SV_Fmt "\n", SV_Arg(sv));
    } else {
        node_pool_get(&ctx->nodes, ctx->current)->explored = true;
        explore_children(ctx->strArena, ctx->client, &ctx->nodes, ctx->current);
        visit_direct_children(ctx->tempArena, &sb, &ctx->nodes, ctx->current, visit_print, NULL);
        String_View sv = sb_to_sv(sb);
        printf(SV_Fmt "\n", SV_Arg(sv));
    }
}

void cmd_cd(void* userdata, const char* args) {
    context* ctx = (context*)userdata;
    String_Builder sb = {0};

    if (strcmp(args, "..") == 0) {
        cmd_cd_up(ctx, "");
    }

    // TARGET NODE
    int index;
    if (sscanf(args, "%d", &index) != 1) {
        log_warn("No such node available");
        return;
    }
    if (index < 0) {
        log_warn("negative value not supported");
        return;
    }

    NodeRef firstChild = node_pool_get(&ctx->nodes, ctx->current)->firstChildRef;
    if (node_cmp(firstChild, NODE_NILL)) {
        log_warn("No Children!");
        return;
    }

    uint32_t i = 1;
    NodeRef current = firstChild;
    do {
        UaNodeIdExpanded* n = node_pool_get(&ctx->nodes, current);
        if ((uint32_t)index == i) {
            break;
        }
        current = n->nextSiblingRef;
        i++;
    } while (!node_cmp(current, firstChild));

    UaNodeIdExpanded* target = node_pool_get(&ctx->nodes, current);
    ctx->current = current;
    if (!target->explored) {
        target->explored = true;
        explore_children(ctx->strArena, ctx->client, &ctx->nodes, current);
    }
    cmd_ls(ctx, NULL);
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
    //     printf(SV_Fmt "\n", SV_Arg(sv));
    //
}
static void mirua_save2(context* ctx, int start, int end) {
    // TOOO: check that it is connected.
    //  if (!ctx->connected) {
    //      log_error("UA_Client not connected!");
    //      return;
    //  }

    // GETTING RANGE OF NODES
    // mirua_t_NodeList subset;
    assert(start > 0 || end > 0 && "missing start and end");
    assert(start <= end && "end cannot be bigger than start");

    // NODE collection for range
    //
    // size_t range_size =
    //     (end >= start && (size_t)end < ctx->currentChildren.size) ? (size_t)(end - start + 1)
    // :
    //     0;
    size_t count = end - start;

    log_trace("[SAVE] supplied with range %i - %i", start, end);
    // mirua_init_nodeList(&subset, range_size);
    //
    //
    // iterator NodeRef
    // for(NodeRef ref = ctx->current; )

    NodeChildIter it = node_children_begin(&ctx->nodes, ctx->current);
    size_t i = 1;
    arr_NodeRef refs = {0};
    for (;;) {
        UaNodeIdExpanded* child = node_children_next(&it);
        if (!child) break;

        if (i >= (size_t)start && i <= (size_t)end) {
            da_arena_append(ctx->tempArena, &refs, it.current, NodeRef);
        }
        i++;
        if (i > (size_t)end) {
            break;
        }

        // OBJECTS and other non variable

        // if (!mirua_nodeId_nodeClass_is(ctx->client, node->nodeid, UA_NODECLASS_VARIABLE)) {
        //     log_warn(
        //         "[SAVE] Not implemented. doenst do anything for nodes that are not variables!");
        //     continue;
        // }

        // if (child->nodeClass == UA_NODECLASS_VARIABLE) {
        //     log_warn(
        //         "[SAVE] Not implemented. doenst do anything for nodes that are not variables!");
        //     continue;
        // }

        // FILTER
        // if (!mirua_filter_is_numeric_or_bool(ctx->client, node->nodeid)) {
        //     log_warn(
        //         "[SAVE:FILTER] Ignoring node %.*s. Not numeric or bool.",
        //         (int)node->name.name.length,
        //         node->name.name.data);
        //     continue;
        // }

        // TODO: add strucutre support later when server to test against
        //  STRUCTURES
        //  if (mirua_nodeId_is_structure(ctx->client, node->nodeid)) {
        //      MiruaTree* tree = mirua_explore_structure(ctx->client, node->nodeid);
        //      if (tree) {
        //          mirua_tree_collect_values(&subset, tree);
        //          mirua_tree_destroy(tree);
        //      }
        //      // PRIMITIVES
        //
        //  }
    }

    // FILE HANDLING
    // const char* filename = ctx->config.output_path ? ctx->config.output_path : "output_file.txt";

    String_Builder filePath = {0};
    fs_sb_get_executable_dir(ctx->tempArena, &filePath);
    sb_arena_append_cstr(ctx->tempArena, &filePath, "/../data/output_file_2026.txt");
    sb_arena_append_null(ctx->tempArena, &filePath);

    const char* mode = "w";

    if (fs_file_exists(filePath.items)) {
        printf(
            "File '%s' already exists. Overwrite (o), Append (a), or New name (n)? ",
            filePath.items);
        char response = getchar();
        while (getchar() != '\n');  // Consume newline if
        if (response == 'a' || response == 'A') {
            mode = "a";
        } else if (response == 'n' || response == 'N') {
            // TODO: implement
            assert(true && "not implemented");
        } else if (response != 'o' && response != 'O') {
            printf("Invalid choice, using default (overwrite).\n");
        }
    }

    FILE* f = fopen(filePath.items, mode);
    if (!f) {
        log_error(
            "[SAVE] Failed to open file '%s' for %s",
            filePath.items,
            mode[0] == 'a' ? "appending" : "writing");
        return;
    }

    // SERIALIZING
    printf(
        "[SAVE] Serializing children from %d to %d to '%s' (%s)\n",
        start,
        end - 1,
        filePath.items,
        mode[0] == 'a' ? "appending" : "overwriting");
    // MiruaNodeSerializer* serializer = mirua_serializer_get(SERIALIZER_TELEGRF);
    String_Builder sb = {0};
    telegraf_serialize_nodes(ctx->tempArena, &sb, &ctx->nodes, &refs);
    fwrite(sb.items, 1, sb.count, f);

    if (telegraf_serialize_nodes_to_file(ctx->tempArena, &ctx->nodes, &refs, f) == SIZE_MAX) {
        log_error("[SAVE] Serialization failed or buffer overflow");
    } else {
        printf(
            "[SAVE] Successfully %s to '%s'\n",
            mode[0] == 'a' ? "appended" : "saved",
            filePath.items);
    }

    fclose(f);
    // mirua_free_nodeList(&subset);
}
void cmd_save(void* userdata, const char* args) {
    // if (strlen(args) == 0) {
    //     log_info("Invalid input. %s", get_command("save", mirua_state_get(ctx))->description);
    //     return;
    // }
    context* ctx = (context*)userdata;

    int idx1 = INT_MAX, idx2 = INT_MAX;
    int parsed = sscanf(args, "%d %d", &idx1, &idx2);
    if (parsed == 1) {
        mirua_save2(ctx, idx1, -1);
    } else if (parsed == 2) {
        mirua_save2(ctx, idx1, idx2);
    } else {
        log_info("Invalid input. %s");  // get_command("save", mirua_state_get(ctx))->description);
    }
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
