#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <replxx.h>

#include "mirua_module.h"

#define COMMAND_COUNT 3
const char* commands[COMMAND_COUNT] = { "connect", "ls", "config" };

void print_welcome(void) {
    printf("=== MirWiz Command Line Interface ===\n");
    printf("Type 'exit' or 'quit' to exit\n");
    printf("======================================\n\n");
}

void completion_callback(const char* prefix, replxx_completions* completions, int* context_len, void* user_data) {
    (void)user_data;
    (void)context_len;
    for (int i = 0; i < COMMAND_COUNT; ++i) {
        if (strncmp(commands[i], prefix, strlen(prefix)) == 0) {
            replxx_add_completion(completions, commands[i]);
        }
    }
}

void dispatch_command(const char* input, MiruaContext* ctx) {
    char cmd[64], arg[256];
    if (sscanf(input, "%63s %255[^\n]", cmd, arg) < 1) return;

    if (strcmp(cmd, "connect") == 0) {
        mirua_connect(ctx, arg);
    } else if (strcmp(cmd, "ls") == 0) {
        mirua_exploreNodes(ctx, "", -1);
    } else if (strcmp(cmd, "config") == 0) {
        mirua_config_print_ctx(ctx);
    } else {
        printf("Unknown command: %s\n", cmd);
    }
}

int main(void) {
    print_welcome();

    MiruaContext* ctx = mirua_module_create();
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

        dispatch_command(input, ctx);
        printf("\n");
    }

    replxx_end(replxx);
    mirua_module_free(ctx);
    return 0;
}