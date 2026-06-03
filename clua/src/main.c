#include <assert.h>
#include <limits.h>
#include <open62541/types.h>
#include <replxx.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>

#include "log.h"
#include "mirua_module.h"
#include "mirua_module_internal.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/client_subscriptions.h"
#include "tinycthread.h"
#define CACHE_SIZE 1024

#define UA_STRING_FMT "%.*s"
#define UA_STRING_ARG(str) ((int)(str).length), (const char*)(str).data
#define UA_STRING_ARG_P(strp) ((int)(strp)->length), (const char*)(strp)->data

typedef struct {
    UA_NodeId nodeId;
    UA_String varName;
    UA_DataValue value;
    UA_StatusCode status;
    UA_DateTime sourceTimestamp;
} mirua_cache_entry;

static mirua_cache_entry cache[CACHE_SIZE];
static int cache_count = 0;
static mtx_t cache_mutex = {0};

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
    UA_NodeId* nodeId = (UA_NodeId*)monContext;
    UA_String str = {0};
    UA_NodeId_print(nodeId, &str);
    if (value->hasValue) {
        log_trace("callback from %u node:%.*s", subId, (int)str.length, str.data);
        mtx_lock(&cache_mutex);
        if (cache_count < CACHE_SIZE) {
            cache[cache_count].nodeId = *nodeId;
            UA_DataValue_copy(value, &cache[cache_count].value);
            cache[cache_count].status = value->status;
            cache[cache_count].sourceTimestamp = value->sourceTimestamp;
            cache_count++;
        }
        mtx_unlock(&cache_mutex);
    }
    UA_String_clear(&str);
}

int mirua_subscription_create(UA_Client* client, UA_NodeId nodeId, UA_NodeId second) {
    // Return ids that were valid. for deleting in future
    // TODO: fix leaks / danling pointers

    // Create a subscription request
    UA_CreateSubscriptionRequest subRequest = UA_CreateSubscriptionRequest_default();
    UA_CreateSubscriptionResponse subResponse =
        UA_Client_Subscriptions_create(client, subRequest, NULL, NULL, NULL);

    if (subResponse.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        log_error(
            "Failed to create subscription: %s\n",
            UA_StatusCode_name(subResponse.responseHeader.serviceResult));
        return 0;
    }
    UA_UInt32 subId = subResponse.subscriptionId;

    UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(nodeId);

    UA_MonitoredItemCreateResult monRet = UA_Client_MonitoredItems_createDataChange(
        client,
        subId,
        UA_TIMESTAMPSTORETURN_BOTH,
        monRequest,
        (void*)&nodeId,
        handler_TheAnswerChanged,
        NULL);

    if (monRet.statusCode == UA_STATUSCODE_GOOD) {
        log_trace("Subscription created, subId=%u\n", subId);
    } else {
        log_error("Failed to create monitored item: %s\n", UA_StatusCode_name(monRet.statusCode));
        return 0;
    }
    UA_MonitoredItemCreateRequest monRequest2 = UA_MonitoredItemCreateRequest_default(second);

    UA_MonitoredItemCreateResult monRet2 = UA_Client_MonitoredItems_createDataChange(
        client,
        subId,
        UA_TIMESTAMPSTORETURN_BOTH,
        monRequest2,
        (void*)&second,
        handler_TheAnswerChanged,
        NULL);

    if (monRet2.statusCode == UA_STATUSCODE_GOOD) {
        log_trace("Subscription created, subId=%u\n", subId);
    } else {
        log_error("Failed to create monitored item: %s\n", UA_StatusCode_name(monRet.statusCode));
        return 0;
    }
    return 1;
}

int iterate_thread(void* ctx) {
    sqlite3* db;
    char* err_msg = 0;
    const char* dbName = "trace_new.db";
    mtx_init(&cache_mutex, mtx_plain);
    int rc = sqlite3_open(dbName, &db);
    if (rc != SQLITE_OK) {
        printf("Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    log_trace("Created sqlite3 connection to %s", dbName);
    const char* sql_create =
        "CREATE TABLE IF NOT EXISTS plc_variable ("
        "id INTEGER PRIMARY KEY, "
        "nodeid TEXT NOT NULL UNIQUE, "
        "name TEXT, "
        "datatype TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS plc_data ("
        "variable_id INTEGER NOT NULL, "
        "epoch_ms INTEGER NOT NULL, "
        "value REAL, "
        "value2 REAL, "
        "status TEXT, "
        "PRIMARY KEY(variable_id, epoch_ms), "
        "FOREIGN KEY(variable_id) REFERENCES plc_variable(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_plc_data_var_epoch "
        "ON plc_data(variable_id, epoch_ms);";
    rc = sqlite3_exec(db, sql_create, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    log_trace("Created sqlite table");

    // Create a new raw open62541 client for testing
    UA_Client* testClient = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(testClient));

    UA_StatusCode retval = UA_Client_connect(testClient, "opc.tcp://127.0.0.1:4840");
    if (retval != UA_STATUSCODE_GOOD) {
        log_error("Test client failed to connect: %s", UA_StatusCode_name(retval));
        UA_Client_delete(testClient);
        return 1;
    }

    UA_NodeId nodeId = UA_NODEID_STRING(5, "::AsGlobalPV:Basetim");
    UA_NodeId secondNode = UA_NODEID_STRING(5, "::Basetime:ticks");

    mirua_subscription_create(testClient, nodeId, secondNode);

    time_t last_log = 0;
    while (g_running) {
        UA_Client_run_iterate(testClient, 100);
        if (cache_count <= 0) {
            log_trace("Cache is empty");
            continue;
        } else {
            log_trace("cache has stuff!");
        }
        mtx_lock(&cache_mutex);
        sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

        UA_String nodeIdStr;
        UA_String_init(&nodeIdStr);
        for (int i = 0; i < cache_count; ++i) {
            UA_DataValue* entryData = &cache[i].value;
            UA_NodeId* EntryNodeId = &cache[i].nodeId;

            // char buf[128];

            UA_String_clear(&nodeIdStr);
            UA_NodeId_print(&cache[i].nodeId, &nodeIdStr);

            char buf[1024];
            switch (EntryNodeId->identifierType) {
                case UA_NODEIDTYPE_NUMERIC: {
                    snprintf(buf, 1023, "%u", EntryNodeId->identifier.numeric);
                } break;

                case UA_NODEIDTYPE_STRING: {
                    snprintf(
                        buf, 1023, UA_STRING_FMT, UA_STRING_ARG(EntryNodeId->identifier.string));
                } break;
                default: {
                    // TODO: move somehwere nice. for now just inline
                    printf("This part supports only nodeid type numeric and string");
                } break;
            }
            // mirua_to_string_nodeId(buf, 128, testClient, &cache[i].nodeId);
            log_trace("nodeid:" UA_STRING_FMT, UA_STRING_ARG(nodeIdStr));

            double val = 0.0f;
            if (UA_Variant_isScalar(&entryData->value)) {
                const UA_DataType* type = entryData->value.type;

                if (type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
                    val = *(UA_Boolean*)entryData->value.data ? 1.0 : 0.0;
                } else if (type == &UA_TYPES[UA_TYPES_SBYTE]) {
                    val = (double)*(UA_SByte*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_BYTE]) {
                    val = (double)*(UA_Byte*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_INT16]) {
                    val = (double)*(UA_Int16*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_UINT16]) {
                    val = (double)*(UA_UInt16*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_INT32]) {
                    val = (double)*(UA_Int32*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_UINT32]) {
                    val = (double)*(UA_UInt32*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_INT64]) {
                    val = (double)*(UA_Int64*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_UINT64]) {
                    val = (double)*(UA_UInt64*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_FLOAT]) {
                    val = (double)*(UA_Float*)entryData->value.data;
                } else if (type == &UA_TYPES[UA_TYPES_DOUBLE]) {
                    val = *(UA_Double*)entryData->value.data;
                } else {
                    val = -999.0;
                }

                UA_DateTimeStruct dts = UA_DateTime_toStruct(cache[i].sourceTimestamp);
                UA_Int64 epoch_ms = UA_DateTime_toUnixTime(cache[i].sourceTimestamp) * 1000LL +
                    (UA_Int64)dts.milliSec;

                int variable_id = -1;
                sqlite3_stmt* stmt = NULL;

                /* Ensure variable exists */
                sqlite3_prepare_v2(
                    db,
                    "INSERT OR IGNORE INTO plc_variable(nodeid, name, datatype) VALUES(?, ?, ?);",
                    -1,
                    &stmt,
                    NULL);
                sqlite3_bind_text(stmt, 1, buf, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, buf, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(
                    stmt,
                    3,
                    entryData->value.type ? entryData->value.type->typeName : "unknown",
                    -1,
                    SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                /* Look up variable_id */
                sqlite3_prepare_v2(
                    db, "SELECT id FROM plc_variable WHERE nodeid = ?;", -1, &stmt, NULL);
                sqlite3_bind_text(stmt, 1, buf, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    variable_id = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);

                if (variable_id > 0) {
                    sqlite3_prepare_v2(
                        db,
                        "INSERT OR REPLACE INTO plc_data(variable_id, epoch_ms, value, value2, "
                        "status) "
                        "VALUES(?, ?, ?, ?, ?);",
                        -1,
                        &stmt,
                        NULL);
                    sqlite3_bind_int(stmt, 1, variable_id);
                    sqlite3_bind_int64(stmt, 2, epoch_ms);
                    sqlite3_bind_double(stmt, 3, val);
                    sqlite3_bind_double(stmt, 4, val + 1000);
                    sqlite3_bind_text(
                        stmt, 5, UA_StatusCode_name(cache[i].status), -1, SQLITE_TRANSIENT);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                }

                // UA_NodeId_clear(&cache[i].nodeId);
                // UA_DataValue_clear(&cache[i].value);
            }
        }
        sqlite3_exec(db, "END TRANSACTION;", 0, 0, 0);
        cache_count = 0;
        mtx_unlock(&cache_mutex);
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
