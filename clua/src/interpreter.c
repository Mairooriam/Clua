#include "interpreter.h"

#include <log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "mirua_module_internal.h"
#include "mirua_types.h"
#include "modules/mirua_module.h"
// #include "modules/file_module.h"

Interpreter* interpreter_create_ast(void) {
    Interpreter* interp = malloc(sizeof(Interpreter));
    if (!interp) return NULL;

    // interp->help_context = help_module_create();
    // interp->file_context = file_module_create();
    interp->mirua_context = mirua_module_create();

    return interp;
}

void interpreter_free_ast(Interpreter* interpreter) {
    if (interpreter) {
        // help_module_free(interpreter->help_context);
        // file_module_free(interpreter->file_context);
        mirua_module_free(interpreter->mirua_context);

        free(interpreter);
    }
}

void interpreter_execute_ast(Interpreter* interpreter, AST* ast) {
    if (!ast || !interpreter) return;

    for (size_t i = 0; i < ast->node_count; i++) {
        ASTNode* node = ast->nodes[i];
        if (!node) continue;

        MiruaState state = interpreter_get_mirua_state(interpreter);
        if (state == MIRUA_STATE_NORMAL) {
            interpreter_normal_state_execute(interpreter, node);
        } else if (state == MIRUA_STATE_CONFIG) {
            interpreter_config_state_execute(interpreter, node);
        } else {
            log_warn("[EXECUTE] - unkown state.");
        }
    }
}
void interpreter_normal_state_execute(Interpreter* interpreter, ASTNode* node) {
    switch (node->type) {
        case AST_NODE_STATE: {
            mirua_state_change(interpreter->mirua_context, MIRUA_STATE_CONFIG);
            mirua_config_print_ctx(interpreter->mirua_context);
        } break;
        case AST_CONNECT: {
            const char* ip = NULL;
            for (size_t j = 0; j < node->child_count; j++) {
                ASTNode* child = node->children[j];
                if (child->type == AST_IP) {
                    ip = child->data.parameter.value.s;
                }
            }
            log_trace("[INTERPRETER] - CONNECT");
            mirua_connect(interpreter->mirua_context, ip);
        } break;
        case AST_LIST_DIRS: {
            log_trace("[INTERPRETER] - LIST DIRS");
            int idx = node->data.list_dirs.idx;

            ASTNodeIdData nodeid;
            int nodeid_found = 0;
            for (size_t i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_NODEID) {
                    nodeid = node->children[i]->data.nodeid;
                    nodeid_found = 1;
                }
            }

            if (nodeid_found) {
                mirua_exploreNodes(
                    interpreter->mirua_context,
                    nodeid.str,
                    -1);  // TODO: make function to take actual nodeid elements
            } else {
                mirua_exploreNodes(interpreter->mirua_context, "", idx);
            }

        } break;
        case AST_NAVIGATE_UP: {
            log_trace("[INTERPRETER] - NAVIGATE UP");
            mirua_navigate_up(interpreter->mirua_context);
        }
        case AST_NAVIGATE_TO: {
            for (size_t i = 0; i < node->child_count; i++) {
                ASTNode* child = node->children[i];
                if (child->type == AST_NODEID) {
                    log_trace("[INTERPRETER] - NAVIGATE TO");
                    mirua_navigate_to(interpreter->mirua_context, child->data.nodeid.str);
                }
            }

        } break;
        case AST_NAVIGATE_DOWN: {
            for (size_t i = 0; i < node->child_count; i++) {
                ASTNode* child = node->children[i];
                if (child->type == AST_PARAMETER) {
                    if (child->data.parameter.type == AST_PARAM_TYPE_INT) {
                        log_trace("[INTERPRETER] - NAVIGATE DOWN");
                        mirua_navigate_down(
                            interpreter->mirua_context, child->data.parameter.value.i);
                    }
                }
            }

        } break;
        case AST_NODE_SAVE: {
            for (size_t i = 0; i < node->child_count; i++) {
                ASTNode* child = node->children[i];
                if (child->type == AST_RANGE) {
                    log_trace("[INTERPRETER] - SAVE");
                    mirua_save(
                        interpreter->mirua_context, child->data.range.start, child->data.range.end);
                }
            }
        } break;
            // case TOKEN_CURRENT:
            // case TOKEN_SETTINGS:
            // case TOKEN_HISTORY:
            // case TOKEN_SAVE:
            // mirua_execute(interpreter->mirua_context, node);
            // file_browse_execute(interpreter->file_context, node);
            break;
        default: log_warn("[INTERPRETER] Unkown node: %d\n", node->type); break;
    }
}
void interpreter_config_state_execute(Interpreter* interpreter, ASTNode* node) {
    (void)interpreter;
    (void)node;
    switch (node->type) {
        case AST_NODE_STATE: {
            mirua_state_change(interpreter->mirua_context, MIRUA_STATE_NORMAL);
            mirua_print_current_node(interpreter->mirua_context);
            mirua_print_current_children(interpreter->mirua_context);
        } break;
        case AST_LIST_DIRS: {
            log_trace("[INTERPRETER] - LIST DIRS (CONFIG)");
            int idx = node->data.list_dirs.idx;

            // In config state, only show config based on idx (no node exploration)
            if (idx != -1) {
                mirua_config_print_by_idx(interpreter->mirua_context, idx);
            } else {
                mirua_config_print_ctx(interpreter->mirua_context);
            }
        } break;
        case AST_NODE_SET: {
            if (node->child_count < 2) {
                log_warn("[CONFIG] Set command needs index/key and value");
                return;
            }

            ASTNode* first_child = node->children[0];
            ASTNode* value_child = node->children[1];

            if (value_child->type != AST_PARAMETER ||
                value_child->data.parameter.type != AST_PARAM_TYPE_STRING) {
                log_warn("[CONFIG] Invalid value for set command");
                return;
            }
            const char* value = value_child->data.parameter.value.s;

            if (first_child->type != AST_PARAMETER) {
                log_warn("[CONFIG] Invalid first parameter for set command");
                return;
            }

            if (first_child->data.parameter.type == AST_PARAM_TYPE_INT) {
                // Index-based: set by index
                size_t idx = first_child->data.parameter.value.i;
                mirua_config_set_by_idx_ctx(interpreter->mirua_context, idx, value);
                log_trace("[CONFIG] Set index %zu to %s", idx, value);
            } else if (first_child->data.parameter.type == AST_PARAM_TYPE_STRING) {
                // Key-based: set by key
                const char* key = first_child->data.parameter.value.s;
                mirua_config_set_ctx(interpreter->mirua_context, key, value);
                log_trace("[CONFIG] Set %s to %s", key, value);
            } else {
                log_warn("[CONFIG] First parameter must be int (index) or string (key)");
            }
        } break;
        default: {
            log_warn("[INTERPRETER] - Not handled ast node\n");
        } break;
    }
}

MiruaState interpreter_get_mirua_state(Interpreter* interpreter) {
    return mirua_state_get(interpreter->mirua_context);
}
