#include <replxx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mirua_module.h"

// Forward declarations for command handlers
void cmd_connect(MiruaContext* ctx, const char* args);
void cmd_ls(MiruaContext* ctx, const char* args);
void cmd_config(MiruaContext* ctx, const char* args);
void cmd_cd(MiruaContext* ctx, const char* args);
void cmd_cd_up(MiruaContext* ctx, const char* args);
void cmd_save(MiruaContext* ctx, const char* args);
// Command structure
typedef struct {
    const char* name;
    const char* description;
    void (*handler)(MiruaContext* ctx, const char* args);
} Command;

// Command table
static const Command commands[] = {
    {"connect", "Connect to OPC-UA server", cmd_connect},
    {"con", "Connect to OPC-UA server (alias)", cmd_connect},
    {"ls", "List nodes", cmd_ls},
    {"config", "Show configuration", cmd_config},
    {"cd", "Change directory", cmd_cd},
    {"cd..", "Go up one level", cmd_cd_up},
    {"save", "saves nodes for telegraf format", cmd_save},
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

void print_welcome(void) {
    printf("=== MirWiz Command Line Interface ===\n");
    printf("Type 'exit' or 'quit' to exit\n");
    printf("======================================\n\n");
}

void completion_callback(
    const char* prefix, replxx_completions* completions, int* context_len, void* user_data) {
    (void)user_data;

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
            if (strncmp(commands[i].name, word_start, word_len) == 0) {
                replxx_add_completion(completions, commands[i].name);
            }
        }
    }
}

void dispatch_command(const char* input, MiruaContext* ctx) {
    char cmd[64], arg[256];
    int args_parsed = sscanf(input, "%63s %255[^\n]", cmd, arg);

    if (args_parsed < 1) return;

    // Find and execute command
    for (size_t i = 0; i < COMMAND_COUNT; ++i) {
        if (strcmp(cmd, commands[i].name) == 0) {
            const char* args_str = (args_parsed >= 2) ? arg : "";
            commands[i].handler(ctx, args_str);
            return;
        }
    }

    printf("Unknown command: %s\n", cmd);
}

// Command handlers
void cmd_connect(MiruaContext* ctx, const char* args) {
    const char* endpoint = strlen(args) > 0 ? args : "opc.tcp://127.0.0.1:4840";
    mirua_connect(ctx, endpoint);
}

void cmd_ls(MiruaContext* ctx, const char* args) {
    (void)args;
    mirua_exploreNodes(ctx, "", -1);
}

void cmd_config(MiruaContext* ctx, const char* args) {
    (void)args;
    mirua_config_print_ctx(ctx);
}

void cmd_cd(MiruaContext* ctx, const char* args) {
    if (strlen(args) == 0) {
        printf("Usage: cd <index>\n");
        return;
    }

    char* endptr;
    long index = strtol(args, &endptr, 10);

    if (*endptr != '\0' || endptr == args) {
        printf("Error: Invalid index '%s'\n", args);
        return;
    }

    if (index < 0 || index > INT_MAX) {
        printf("Error: Index out of range\n");
        return;
    }

    mirua_navigate_down(ctx, (int)index);
}

void cmd_cd_up(MiruaContext* ctx, const char* args) {
    (void)ctx;
    (void)args;
    // TODO: implement cd..
    printf("cd.. command not yet implemented\n");
}

void cmd_save(MiruaContext* ctx, const char* args) {
    printf("save not implemented\n");
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
