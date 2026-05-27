#include <assert.h>
#include <limits.h>
#include <replxx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "log.h"
#include "mirua_module.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/client_subscriptions.h"
static volatile int g_running = 1;
static mtx_t log_mutex;
static mtx_t client_mutex;
static void log_lock_fn(bool lock, void* udata) {
    if (lock)
        mtx_lock((mtx_t*)udata);
    else
        mtx_unlock((mtx_t*)udata);
}
static void handler_TheAnswerChanged(
    UA_Client* client,
    UA_UInt32 subId,
    void* subContext,
    UA_UInt32 monId,
    void* monContext,
    UA_DataValue* value) {
    if (value->hasValue) {
        printf("Subscription value changed: ");
        // UA_Variant_print(&value->value);
        printf("\n");
    }
}
int iterate_thread(void* ctx) {
    // Create a new raw open62541 client for testing
    UA_Client* testClient = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(testClient));

    UA_StatusCode retval = UA_Client_connect(testClient, "opc.tcp://192.168.0.10:4840");
    if (retval != UA_STATUSCODE_GOOD) {
        log_error("Test client failed to connect: %s", UA_StatusCode_name(retval));
        UA_Client_delete(testClient);
        return 1;
    }

    UA_NodeId nodeId = UA_NODEID_STRING(6, "::AsGlobalPV:Basetim");

    // Create a subscription request
    UA_CreateSubscriptionRequest subRequest = UA_CreateSubscriptionRequest_default();
    UA_CreateSubscriptionResponse subResponse =
        UA_Client_Subscriptions_create(testClient, subRequest, NULL, NULL, NULL);

    if (subResponse.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
        UA_UInt32 subId = subResponse.subscriptionId;

        UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(nodeId);

        UA_MonitoredItemCreateResult monRet = UA_Client_MonitoredItems_createDataChange(
            testClient,
            subId,
            UA_TIMESTAMPSTORETURN_BOTH,
            monRequest,
            NULL,
            handler_TheAnswerChanged,
            NULL);

        if (monRet.statusCode == UA_STATUSCODE_GOOD) {
            printf("Subscription created, subId=%u\n", subId);
        } else {
            printf("Failed to create monitored item: %s\n", UA_StatusCode_name(monRet.statusCode));
        }
    } else {
        printf(
            "Failed to create subscription: %s\n",
            UA_StatusCode_name(subResponse.responseHeader.serviceResult));
    }

    time_t last_log = 0;
    while (g_running) {
        // Test client iterate
        UA_Client_run_iterate(testClient, 100);

        // (Optional) Also run your main client logic if needed
        // mtx_lock(&client_mutex);
        // mirua_client_iterate(ctx, 100);
        // mtx_unlock(&client_mutex);

        // log_trace("iterate_thread running (test client)");

        time_t now = time(NULL);
        if (now - last_log >= 2) {
            last_log = now;
        }
    }

    UA_Client_disconnect(testClient);
    UA_Client_delete(testClient);
    return 0;
}
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
            if (commands[i].context == mirua_state_get(ctx)) {
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
        if (commands[i].context == mirua_state_get(ctx)) {
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
    const char* endpoint = strlen(args) > 0 ? args : mirua_config_get_current_endpoint(ctx);
    log_info("endpoint:%s", endpoint);
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
            log_error(
                "Invalid input. { %s }", get_command("ls", mirua_state_get(ctx))->description);
        }
    }
}

void cmd_config_edit(MiruaContext* ctx, const char* args) {
    // TODO: if too tidious move into some lexer stuff if needed
    int index;
    char value[128];

    if (sscanf(args, "%d %127[^\n]", &index, value) != 2) {
        log_error("Invalid input. { %s }", get_command("edit", mirua_state_get(ctx))->description);
    }

    mirua_config_set_by_idx_ctx(ctx, index, value);
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
        log_info("Invalid input. %s", get_command("cd", mirua_state_get(ctx))->description);
        return;
    }

    int index;
    if (sscanf(args, "%d", &index) != 1) {
        log_info("Invalid input. %s", get_command("cd", mirua_state_get(ctx))->description);
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
    (void)ctx;
    (void)args;

    // TODO: implement platform specific stuff to copy to clipboard Q1  Q!2A~
}

void cmd_save(MiruaContext* ctx, const char* args) {
    if (strlen(args) == 0) {
        log_info("Invalid input. %s", get_command("save", mirua_state_get(ctx))->description);
        return;
    }

    int idx1 = INT_MAX, idx2 = INT_MAX;
    int parsed = sscanf(args, "%d %d", &idx1, &idx2);
    if (parsed == 1) {
        mirua_save(ctx, idx1, -1, -1);
    } else if (parsed == 2) {
        mirua_save(ctx, -1, idx1, idx2);
    } else {
        log_info("Invalid input. %s", get_command("save", mirua_state_get(ctx))->description);
    }
}

int main(void) {
    print_welcome();

    MiruaContext* ctx = mirua_module_create();
    Replxx* replxx = replxx_init();

    // Set autocomplete callback
    replxx_set_completion_callback(replxx, completion_callback, ctx);

    thrd_t t;
    mtx_init(&log_mutex, mtx_plain);
    mtx_init(&client_mutex, mtx_plain);
    log_set_lock(log_lock_fn, &log_mutex);
    thrd_create(&t, iterate_thread, ctx);

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

        mtx_lock(&client_mutex);
        dispatch_command(input, ctx);
        mtx_unlock(&client_mutex);
        printf("\n");
    }

    g_running = 0;
    thrd_join(t, NULL);
    mtx_destroy(&log_mutex);
    mtx_destroy(&client_mutex);
    replxx_end(replxx);
    mirua_module_free(ctx);
    return 0;
}
