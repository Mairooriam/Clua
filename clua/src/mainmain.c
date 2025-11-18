#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "interpreter.h"
#include "lexer.h"
#include "mirua_module.h"
#include "mirua_types_internal.h"
#include "modules/file/file_module.h"
#include "open62541/types.h"
#include "parser.h"
#define MAX_INPUT_SIZE 1024
const char* CONFIG_PATH = "\\config";
const char* CONFIG_FILENAME = "config.mir";

#include <open62541/client.h>
#include <open62541/client_config_default.h>

#include "mirua_module_internal.h"
#include "mirua_types.h"
void process_input(const char* input, Interpreter* interpreter) {
    if (!input || strlen(input) == 0) return;

    int token_count;
    Token** tokens = lexer_tokenize_all(input, &token_count);
    if (!tokens) {
        printf("Error: Failed to tokenize input\n");
        return;
    }

    Parser* parser = parser_create(tokens, token_count);
    if (!parser) {
        printf("Error: Failed to create parser\n");
        lexer_free_tokens(tokens, token_count);
        return;
    }

    AST* ast = parser_parse(parser);
    if (!ast) {
        printf("Error: Failed to parse tokens\n");
        parser_free(parser);
        lexer_free_tokens(tokens, token_count);
        return;
    }

    interpreter_execute_ast(interpreter, ast);

    ast_free(ast);
    parser_free(parser);
    lexer_free_tokens(tokens, token_count);
}

char* trim_whitespace(char* str) {
    char* end;

    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';

    return str;
}

void print_welcome(void) {
    printf("=== MirWiz Command Line Interface ===\n");
    printf("Type 'help' for available commands\n");
    printf("Type 'exit' or 'quit' to exit\n");
    printf("======================================\n\n");
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "--test") == 0) {
        // Run tests directly
        MiruaContext* ctx = mirua_module_create();

        // mirua_exploreNodes(ctx, "", 0);
        // MiruaConfig* config = &ctx->config;
        // mirua_config_set(config, "endpoint", "opc.tcp://test:4840");
        // mirua_config_set(config, "filter", "lol");
        // mirua_config_set(config, "filter_type", "Lol2");
        // mirua_config_print_ctx(ctx);
        // mirua_config_set_by_idx(config, 0, "opc.tcp://Set_by_idx");
        // mirua_config_set_by_idx(config, 1, "ns=5;i=20000");
        // mirua_config_set_by_idx(config, 2, "1-15");
        // mirua_config_set_by_idx(config, 3, "ALL");
        // mirua_config_print_ctx(ctx);
        // mirua_config_print_ctx(ctx);
        // mirua_config_load_from_file(&ctx->config, "config.mir");
        // mirua_config_print_ctx(ctx);
        // mirua_state_change(ctx, MIRUA_STATE_NORMAL);
        mirua_connect(ctx, "opc.tcp://127.0.0.1:4840");
        // mirua_explore_children(ctx, &ctx->currentChildren, &ctx->currentNode.nodeid);
        // mirua_print_current_node(ctx);
        // mirua_print_current_children(ctx);
        //
        // UA_String str = UA_String_fromChars("ns=5;s=::Program1:mouse_advv");
        UA_String str = UA_String_fromChars("ns=5;s=::Program1:Mouses");

        UA_NodeId node;
        UA_StatusCode status = UA_NodeId_parse(&node, str);
        // mirua_explore_value(ctx, &node);
        // if (mirua_nodeId_is_structure(ctx->client, node)) {
        //     printf("NODE IS STRUCTURE!");
        //     MiruaTreeNode* treeNode = mirua_tree_node_create(ctx->client, node);
        //     MiruaTree* tree = mirua_tree_create(treeNode);
        //     mirua_build_structure_tree(ctx->client, tree, tree->root, node);
        //     char buf[1024 * 8];
        //     size_t bufsize = sizeof(buf);
        //     mirua_tree_to_string(buf, bufsize, tee);
        //     log_trace("%s", buf);
        // }
        //
        //
        mirua_explore_structure(ctx->client, node);
        mirua_module_free(ctx);
        return 0;
    }

    char* str = mir_utils_getCurrentDirectory();
    if (str == NULL) {
        printf("Failed getting current directory");
        return -1;
    }

    size_t base_len = strlen(str);
    size_t config_len = strlen(CONFIG_PATH);
    size_t filename_len = strlen(CONFIG_FILENAME);
    size_t total_len = base_len + config_len + 1 + filename_len + 1;

    str = realloc(str, total_len);
    if (str == NULL) {
        printf("Memory allocation failed");
        return -1;
    }

    strcat(str, CONFIG_PATH);
    printf("Looking for opcua config in %s\n", str);

    if (mir_utils_fileExistsInDirectory(str, CONFIG_PATH)) {
        printf("config.mir found!\n");
    }

    // mirua_t_NodeList* node = mirua_parseConfig("path");
    // mirua_saveToConfig();

    FILE* fPtr;
    strcat(str, "\\");
    strcat(str, CONFIG_FILENAME);

    fPtr = fopen(str, "r");
    if (fPtr == NULL) {
        printf("file couldn't be opened. File: %s\n", str);
    }

    free(str);

    // if empty config -> promt user to connect to opcua client
    // UA_Client* client = UA_Client_new();
    // UA_ClientConfig_setDefault(UA_Client_getConfig(client));
    //
    // UA_StatusCode retval = UA_Client_connect(client, "opc.tcp://127.0.0.1:4840");
    // if (retval != UA_STATUSCODE_GOOD) {
    //     printf("Could not connect\n");
    //     printf("Closing program\n");
    //     UA_Client_delete(client);
    //     return -1;
    // }

    Interpreter* interpreter = interpreter_create_ast();
    if (!interpreter) {
        printf("Error: Failed to create interpreter\n");
        return 1;
    }

    // Check if command was passed as argument
    if (argc > 1) {
        char input[MAX_INPUT_SIZE] = {0};
        for (int i = 1; i < argc; i++) {
            strcat(input, argv[i]);
            if (i < argc - 1) strcat(input, " ");
        }

        printf("Executing: %s\n", input);
        process_input(input, interpreter);

        interpreter_free_ast(interpreter);
        return 0;
    }

    print_welcome();
    char input[MAX_INPUT_SIZE];
    while (1) {
        const char* prompt = (interpreter_get_mirua_state(interpreter) == MIRUA_STATE_NORMAL)
            ? "mirwiz"
            : "mirwiz>config";
        printf("%s> ", prompt);
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            printf("\nGoodbye!\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';
        char* trimmed = trim_whitespace(input);

        if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0 ||
            strcmp(trimmed, "q") == 0) {
            printf("Goodbye!\n");
            break;
        }

        if (strlen(trimmed) == 0) {
            continue;
        }

        process_input(trimmed, interpreter);
        printf("\n");
    }

    interpreter_free_ast(interpreter);
    return 0;
}
