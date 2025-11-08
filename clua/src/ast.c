#include "ast.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_buf.h"
#include "log.h"

AST* ast_create(void) {
    AST* ast = malloc(sizeof(AST));
    if (ast == NULL) {
        return NULL;
    }
    memset(ast, 0, sizeof(AST));
    log_alloc("[CREATED] - AST");
    return ast;
}

void ast_free(AST* ast) {
    if (ast == NULL) {
        return;
    }
    for (size_t i = 0; i < ast->node_count; i++) {
        ast_node_free(ast->nodes[i]);
    }
    free(ast->nodes);
    free(ast);
    log_de_alloc("[FREED] - AST");
}

ASTNode* ast_create_node(ASTNodeType type, ASTNodeData data) {
    ASTNode* node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    memset(node, 0, sizeof(ASTNode));
    node->type = type;
    node->data = data;
    node->child_capacity = AST_INITIAL_CAPACITY;
    log_alloc("[CREATED] - %s", ast_to_string_nodetype(type));
    return node;
}
void ast_node_free(ASTNode* node) {
    if (!node) return;
    char* buf = dbg_getbuf(0);
    size_t bufsize = dbg_buf_size();
    size_t offset = 0;
    ast_to_string_node(buf, bufsize, &offset, node, 0);
    log_de_alloc("[FREEING AST_NODE] - %s", buf);

    for (size_t i = 0; i < node->child_count; i++) {
        ast_node_free(node->children[i]);
    }

    free(node->children);

    if ((node->type == AST_PARAMETER || node->type == AST_IP) &&
        node->data.parameter.type == AST_PARAM_TYPE_STRING && node->data.parameter.value.s) {
        free(node->data.parameter.value.s);
    }
    free(node);
}

void ast_add_child(ASTNode* parent, ASTNode* child) {
    char* buf = dbg_getbuf(0);
    size_t bufsize = dbg_buf_size();
    size_t offset = 0;

    if (!parent) {
        ast_to_string_node(buf, bufsize, &offset, child, 0);
        log_warn("Tried to add {%s} to an empty parent node!", buf);
        return;
    }
    if (!child) {
        ast_to_string_node(buf, bufsize, &offset, parent, 0);
        log_warn("Tried to add empty child to {%s}", buf);
        return;
    }

    if (parent->children == NULL) {
        ASTNode** tmp = malloc(sizeof(ASTNode*) * parent->child_capacity);
        if (!tmp) {
            log_fatal("Malloc failed on ASTNode");
        }

        parent->children = tmp;
    }

    if (parent->child_count == parent->child_capacity) {
        int new_capacity = parent->child_capacity * AST_GROW_FACTOR;
        ASTNode** tmp = realloc(parent->children, sizeof(ASTNode*) * new_capacity);
        if (!tmp) {
            log_fatal("Realloc failed on ASTNode children");
            return;
        }

        parent->children = tmp;
        parent->child_capacity = new_capacity;
        log_alloc("AST capacity grew");  // TODO: better debug printing
        return;
    }

    parent->children[parent->child_count] = child;
    parent->child_count++;

    // ast_to_string_node(buf, bufsize, &offset, child, 0);
    // offset += snprintf(buf + offset, bufsize - offset, "<- {CHILD} {PARENT} ->");
    // ast_to_string_node(buf, bufsize, &offset, parent, 0);
    log_trace("[ADDED] - child to node");
    // log_trace("[ADD] - %s", buf);  // TODO: better debug printing
}

void ast_add_statement(AST* ast, ASTNode* statement) {
    if (!ast) {
        assert(0 && "Passed null ast!");
        return;
    }
    if (!statement) {
        assert(0 && "passed null statement!");
        return;
    }

    if (ast->nodes == NULL) {
        ast->nodes = malloc(sizeof(ASTNode**) * ast->node_capacity);
    }

    if (ast->node_count == ast->node_capacity) {
        size_t new_capacity = ast->node_capacity + AST_GROW_FACTOR;
        ASTNode** tmp = realloc(ast->nodes, sizeof(ASTNode*) * new_capacity);
        if (!tmp) {
            log_fatal("Realloc failed on AST add");
            return;
        }

        ast->nodes = tmp;
        ast->node_capacity = new_capacity;
    }

    ast->nodes[ast->node_count] = statement;
    ast->node_count++;
    log_trace("[ADDED] - node to ast");
}

ASTNode* ast_create_node_parameter(ASTParameterData data) {
    return ast_create_node(AST_PARAMETER, (ASTNodeData){.parameter = data});
}
ASTNode* ast_create_node_range(int start, int end) {
    ASTRangeData range = {.start = start, .end = end};
    return ast_create_node(AST_RANGE, (ASTNodeData){.range = range});
}
ASTNode* ast_create_node_list_dirs(ASTListDirsData data) {
    return ast_create_node(AST_LIST_DIRS, (ASTNodeData){.list_dirs = data});
}
ASTNode* ast_create_node_nodeId(ASTNodeIdData data) {
    return ast_create_node(AST_NODEID, (ASTNodeData){.nodeid = data});
}

ASTNode* ast_create_node_ip(const char* ip_str) {
    ASTParameterData param_data;
    param_data.type = AST_PARAM_TYPE_STRING;
    param_data.value.s = strdup(ip_str);
    return ast_create_node(AST_IP, (ASTNodeData){.parameter = param_data});
}

ASTNode* ast_create_node_navigate_up(void) {
    return ast_create_node(AST_NAVIGATE_UP, (ASTNodeData){0});
}

ASTNode* ast_create_node_connect(void) {
    return ast_create_node(AST_CONNECT, (ASTNodeData){0});
}

ASTNode* ast_create_node_navigate_down() {
    return ast_create_node(AST_NAVIGATE_DOWN, (ASTNodeData){0});
}

ASTNode* ast_create_node_navigate_to(void) {
    return ast_create_node(AST_NAVIGATE_TO, (ASTNodeData){0});
}
ASTNode* ast_create_node_save(void) {
    return ast_create_node(AST_NODE_SAVE, (ASTNodeData){0});
}

ASTNode* ast_create_node_state(void) {
    return ast_create_node(AST_NODE_STATE, (ASTNodeData){0});
}

ASTNode* ast_create_node_set(void) {
    return ast_create_node(AST_NODE_SET, (ASTNodeData){0});
}

size_t ast_debug_print_node(ASTNode* node, char* buf, size_t bufsize, int indent) {
    if (!node) return 0;
    size_t offset = 0;
    int n = snprintf(buf, bufsize, "%*s", indent * 2, "");
    offset += (n > 0) ? n : 0;

    switch (node->type) {
        // case AST_COMMAND:
        //     n = snprintf(
        //         buf + offset, bufsize - offset, "Command: %s\n",
        //         lexer_to_string_tokentype(node->command_type));
        //     break;
        case AST_PARAMETER:
            n = snprintf(
                buf + offset,
                bufsize - offset,
                "Parameter: %s\n",
                node->data.parameter.value.s ? node->data.parameter.value.s : "NULL");
            break;
        case AST_RANGE:
            n = snprintf(
                buf + offset,
                bufsize - offset,
                "Range: %d..%d\n",
                node->data.range.start,
                node->data.range.end);
            break;
        case AST_IP:
            n = snprintf(
                buf + offset,
                bufsize - offset,
                "IP: %s\n",
                node->data.parameter.value.s ? node->data.parameter.value.s : "NULL");
            break;
        default: n = snprintf(buf + offset, bufsize - offset, "Unknown node type\n"); break;
    }
    offset += (n > 0) ? n : 0;

    for (size_t i = 0; i < node->child_count; i++) {
        offset +=
            ast_debug_print_node(node->children[i], buf + offset, bufsize - offset, indent + 1);
    }
    return offset;
}

void ast_to_string_ast(char* buf, size_t bufsize, size_t* offset, AST* ast) {
    *offset += snprintf(buf + *offset, bufsize - *offset, "AST (%zu nodes):\n", ast->node_count);

    for (size_t i = 0; i < ast->node_count; i++) {
        ast_to_string_node(buf, bufsize, offset, ast->nodes[i], 0);
    }
}

void ast_to_string_node(
    char* buf, size_t bufsize, size_t* offset, ASTNode* node, int indent_level) {
    char indent[64] = {0};
    for (int i = 0; i < indent_level * 2; i++) {
        indent[i] = ' ';
    }

    // Print node type for debugging
    *offset += snprintf(
        buf + *offset,
        bufsize - *offset,
        "%sNodeType: %s\n",
        indent,
        ast_to_string_nodetype(node->type));

    if (node->type == AST_PARAMETER) {
        *offset += snprintf(
            buf + *offset,
            bufsize - *offset,
            "%sParameter: %s",
            indent,
            "parameter print not done!");
    } else if (node->type == AST_RANGE) {
        *offset += snprintf(
            buf + *offset,
            bufsize - *offset,
            "%sRange: %d..%d",
            indent,
            node->data.range.start,
            node->data.range.end);
    } else if (node->type == AST_IP) {
        *offset += snprintf(
            buf + *offset,
            bufsize - *offset,
            "%sIP: %s",
            indent,
            node->data.parameter.value.s ? node->data.parameter.value.s : "NULL");
    } else if (node->type == AST_LIST_DIRS) {
        *offset += snprintf(buf + *offset, bufsize - *offset, "%sListDirs:", indent);
    } else if (node->type == AST_CONNECT) {
        *offset += snprintf(buf + *offset, bufsize - *offset, "%sConnect", indent);
    } else if (node->type == AST_DISCONNECT) {
        *offset += snprintf(buf + *offset, bufsize - *offset, "%sDisconnect", indent);
    } else if (node->type == AST_NODEID) {
        *offset += snprintf(buf + *offset, bufsize - *offset, "%sNodeId", indent);
    } else if (node->type == AST_NAVIGATE_UP) {
        *offset += snprintf(buf + *offset, bufsize - *offset, "%sNavigateUp", indent);
    } else if (node->type == AST_NAVIGATE_DOWN) {
        *offset += snprintf(buf + *offset, bufsize - *offset, "%sNavigateDown", indent);
    } else if (node->type == AST_NAVIGATE_TO) {
        *offset += snprintf(buf + *offset, bufsize - *offset, "%sNavigateTo", indent);
    } else {
        *offset += snprintf(
            buf + *offset,
            bufsize - *offset,
            "%sUnknown node type ast.c ast_to_string_node",
            indent);
        log_warn("[TO STRING] - missing printing for this type");
    }
    for (size_t i = 0; i < node->child_count; i++) {
        *offset += snprintf(buf + *offset, bufsize - *offset, "[CHILD_%zu]\n", i);
        ast_to_string_node(buf, bufsize, offset, node->children[i], indent_level + 1);
    }
}
void ast_print_debug_node(ASTNode* node) {
    char* buf = dbg_getbuf(0);
    size_t bufsize = dbg_buf_size();
    size_t offset = 0;
    ast_to_string_node(buf, bufsize, &offset, node, 0);
    printf("%s", buf);
}

void ast_print_debug_ast(AST* ast) {
    if (!ast) return;
    printf("AST (%zu nodes):\n", ast->node_count);
    for (size_t i = 0; i < ast->node_count; i++) {
        ast_print_debug_node(ast->nodes[i]);
    }
}

const char* ast_to_string_nodetype(ASTNodeType type) {
    switch (type) {
        case AST_CONNECT: return "AST_CONNECT";
        case AST_LIST_DIRS: return "AST_LIST_DIRS";
        case AST_DISCONNECT: return "AST_DISCONNECT";
        case AST_PARAMETER: return "AST_PARAMETER";
        case AST_RANGE: return "AST_RANGE";
        case AST_IP: return "AST_IP";
        case AST_NODEID: return "AST_NODEID";
        case AST_NAVIGATE_UP: return "AST_NAVIGATE_UP";
        case AST_NAVIGATE_DOWN: return "AST_NAVIGATE_DOWN";
        case AST_NAVIGATE_TO: return "AST_NAVIGATE_TO";
        case AST_NODE_SAVE: return "AST_NODE_SAVE";
        default: return "UNKNOWN";
    }
}
