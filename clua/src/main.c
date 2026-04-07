#include <assert.h>
#include <limits.h>
#include <replxx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "mirua_module.h"
#include "mirua_module_internal.h"
#include "mirua_types.h"

// Forward declarations for command handlers
void cmd_connect(MiruaContext* ctx, const char* args);
void cmd_ls(MiruaContext* ctx, const char* args);
void cmd_main_change_to_config(MiruaContext* ctx, const char* args);
void cmd_cd(MiruaContext* ctx, const char* args);
void cmd_cd_up(MiruaContext* ctx, const char* args);
void cmd_save(MiruaContext* ctx, const char* args);
void cmd_disconnect(MiruaContext* ctx, const char* args);
void cmd_copy(MiruaContext* ctx, const char* args);

// config context commands
void cmd_config_ls(MiruaContext* ctx, const char* args);
void cmd_config_change_to_main(MiruaContext* ctx, const char* args);
void cmd_config_edit(MiruaContext* ctx, const char* args);

// Command structure
typedef struct {
    const char* name;
    const char* description;
    MiruaState context;
    void (*handler)(MiruaContext* ctx, const char* args);
} Command;

// Command table
static const Command commands[] = {
    // Main context commands
    {"connect", "Connect to OPC-UA server", MIRUA_STATE_NORMAL, cmd_connect},
    {"con", "Connect to OPC-UA server (alias)", MIRUA_STATE_NORMAL, cmd_connect},
    {"ls", "List nodes", MIRUA_STATE_NORMAL, cmd_ls},
    {"config", "Show configuration", MIRUA_STATE_NORMAL, cmd_main_change_to_config},
    {"cd", "cd [idx] | cd <nodeid>", MIRUA_STATE_NORMAL, cmd_cd},
    {"cd..", "Go up one level", MIRUA_STATE_NORMAL, cmd_cd_up},
    {"save", "save <start> <end> | save <index>", MIRUA_STATE_NORMAL, cmd_save},
    {"disconnect", "disconnect <Nothing>", MIRUA_STATE_NORMAL, cmd_disconnect},
    {"copy", "copy <nothing>", MIRUA_STATE_NORMAL, cmd_copy},  // TODO: add more ways to copy?

    // Config context commands
    {"ls", "ls ?[idx]", MIRUA_STATE_CONFIG, cmd_config_ls},
    {"main", "Change to main context", MIRUA_STATE_CONFIG, cmd_config_change_to_main},
    {"edit", "edit [idx] <new value>", MIRUA_STATE_CONFIG, cmd_config_edit}};
#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

const Command* get_command(const char* commandName, MiruaState state) {
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(commands[i].name, commandName) == 0 && commands[i].context == state) {
            return &commands[i];
        }
    }

    assert(0 && "meant for internal use. fix if u call with wrong commandName");
    return NULL;
}

void print_welcome(void) {
    printf("=== MirWiz Command Line Interface ===\n");
    printf("Type 'exit' or 'quit' to exit\n");
    printf("======================================\n\n");
}

void completion_callback(
    const char* prefix, replxx_completions* completions, int* context_len, void* user_data) {
    MiruaContext* ctx = (MiruaContext*)user_data;

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
            if (commands[i].context == ctx->state) {
                if (strncmp(commands[i].name, word_start, word_len) == 0) {
                    replxx_add_completion(completions, commands[i].name);
                }
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
        if (commands[i].context == ctx->state) {
            if (strcmp(cmd, commands[i].name) == 0) {
                const char* args_str = (args_parsed >= 2) ? arg : "";
                commands[i].handler(ctx, args_str);
                return;
            }
        }
    }

    printf("Unknown command: %s\n", cmd);
}

// Command handlers
void cmd_connect(MiruaContext* ctx, const char* args) {
    const char* endpoint = strlen(args) > 0 ? args : ctx->config.endpoint;
    mirua_connect(ctx, endpoint);
}

void cmd_disconnect(MiruaContext* ctx, const char* args) {
    (void)args;
    mirua_disconnect(ctx);
}

void cmd_ls(MiruaContext* ctx, const char* args) {
    (void)args;
    mirua_exploreNodes(ctx, "", -1);
}

void cmd_config_ls(MiruaContext* ctx, const char* args) {
    if (strlen(args) == 0) {
        mirua_config_print_ctx(ctx);
    } else {
        int index;
        if (sscanf(args, "%d", &index) == 1) {
            mirua_config_print_by_idx(ctx, index);
            printf("Hello world");
        } else {
            log_error("Invalid input. { %s }", get_command("ls", ctx->state)->description);
        }
    }
}

void cmd_config_edit(MiruaContext* ctx, const char* args) {
    // TODO: if too tidious move into some lexer stuff if needed
    int index;
    char value[128];

    if (sscanf(args, "%d %127[^\n]", &index, value) != 2) {
        log_error("Invalid input. { %s }", get_command("edit", ctx->state)->description);
    }

    mirua_config_set_by_idx(&ctx->config, index, value);
    printf("%s", args);
}

void cmd_main_change_to_config(MiruaContext* ctx, const char* args) {
    (void)args;
    mirua_state_change(ctx, MIRUA_STATE_CONFIG);
}

void cmd_config_change_to_main(MiruaContext* ctx, const char* args) {
    (void)args;
    mirua_state_change(ctx, MIRUA_STATE_NORMAL);
}

void cmd_cd(MiruaContext* ctx, const char* args) {
    // TODO: add parsing for nodeid
    if (strlen(args) == 0) {
        log_info("Invalid input. %s", get_command("cd", ctx->state)->description);
        return;
    }

    int index;
    if (sscanf(args, "%d", &index) != 1) {
        log_info("Invalid input. %s", get_command("cd", ctx->state)->description);
    } else {
        mirua_navigate_down(ctx, index);
    }
}

void cmd_cd_up(MiruaContext* ctx, const char* args) {
    (void)ctx;
    (void)args;
    // TODO: implement cd..
    mirua_navigate_up(ctx);
}

void cmd_copy(MiruaContext* ctx, const char* args) {
    // TODO: implement platform specific stuff to copy to clipboard Q1  Q!2A~
}

void cmd_save(MiruaContext* ctx, const char* args) {
    if (strlen(args) == 0) {
        log_info("Invalid input. %s", get_command("save", ctx->state)->description);
        return;
    }

    int idx1 = INT_MAX, idx2 = INT_MAX;
    int parsed = sscanf(args, "%d %d", &idx1, &idx2);
    if (parsed == 1) {
        mirua_save(ctx, idx1, -1, -1);
    } else if (parsed == 2) {
        mirua_save(ctx, -1, idx1, idx2);
    } else {
        log_info("Invalid input. %s", get_command("save", ctx->state)->description);
    }
}

int main(void) {
    print_welcome();

    MiruaContext* ctx = mirua_module_create();
    Replxx* replxx = replxx_init();

    // Set autocomplete callback
    replxx_set_completion_callback(replxx, completion_callback, ctx);

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
