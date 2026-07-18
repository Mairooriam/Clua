

#include <open62541/plugin/log_stdout.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/allocator.h"
#include "core/core.h"
#include "core/fileio.h"
#include "core/log.h"
#include "include/db_access.h"
#include "lexer.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/client_highlevel.h"
#include "open62541/client_subscriptions.h"
#include "open62541/types.h"
#include "parser.h"

typedef struct uadb_context {
    UA_Client* client;
    DbContext db;
    da_UA_NodeId* nodes;
    bool connected;
    bool wasDisconnected;
    bool pushBuffer;
    arr_Measurements measCache;
    memory_arena* temporaryArena;
    memory_arena* persistentArena;
    arr_MonitoredItem monitoredDeleteQue;
    uint32_t currentSubId;
    time_t configLastTouch;
} uadb_context;
// static void deleteSubscriptionCallback(
//     UA_Client* client, UA_UInt32 subscriptionId, void* subscriptionContext) {
//     UA_LOG_INFO(
//         UA_Log_Stdout,
//         UA_LOGCATEGORY_APPLICATION,
//         "Subscription Id %u was deleted",
//         subscriptionId);
// }
static void handler_TheAnswerChanged(
    UA_Client* client,
    UA_UInt32 subId,
    void* subContext,
    UA_UInt32 monId,
    void* monContext,
    UA_DataValue* value) {
    uadb_context* ctx = (uadb_context*)UA_Client_getContext(client);
    UA_NodeId* nodeId = (UA_NodeId*)monContext;
    UA_String str = {0};
    UA_NodeId_print(nodeId, &str);

    float val = -999999.0f;
    int64_t timestamp = -1;
    log_info("NodeId Type: %s", value->value.type->typeName);
    if (value->hasValue && value->serverTimestamp && UA_Variant_isScalar(&value->value)) {
        const UA_DataType* type = value->value.type;
        if (type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
            val = *(UA_Boolean*)value->value.data ? 1.0 : 0.0;
        } else if (type == &UA_TYPES[UA_TYPES_SBYTE]) {
            val = (double)*(UA_SByte*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_BYTE]) {
            val = (double)*(UA_Byte*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_INT16]) {
            val = (double)*(UA_Int16*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_UINT16]) {
            val = (double)*(UA_UInt16*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_INT32]) {
            val = (double)*(UA_Int32*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_UINT32]) {
            val = (double)*(UA_UInt32*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_INT64]) {
            val = (double)*(UA_Int64*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_UINT64]) {
            val = (double)*(UA_UInt64*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_FLOAT]) {
            val = (double)*(UA_Float*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_DOUBLE]) {
            val = *(UA_Double*)value->value.data;
        } else {
            MonitoredItem monItem = {.subId = subId, .monId = monId};
            for (size_t i = 0; i < ctx->monitoredDeleteQue.count; i++) {
                MonitoredItem monItem = ctx->monitoredDeleteQue.items[i];

                if (monItem.monId == monId && monItem.subId == subId) {
                    return;
                }
            }
            ARENA_PUSH(ctx->temporaryArena, &ctx->monitoredDeleteQue, MonitoredItem, monItem);

            log_warn(
                "Subscripted to not supported variable type, scheduled removal (sub=%u, mon=%u).",
                subId,
                monId);
            return;
        }
        timestamp = value->serverTimestamp;

        log_trace(
            "callback from %u node:%.*s with value of type:%s, cacheSize:%zu",
            subId,
            (int)str.length,
            str.data,
            value->value.type->typeName,
            ctx->measCache.count);

        size_t name_len = (size_t)str.length;
        int initialMeasSize = 128;
        if (ctx->measCache.count == 0) {
            Measurement* meas = measurement_create_in_arena(
                ctx->temporaryArena, (char*)str.data, name_len, initialMeasSize);
            meas->name = (char*)arena_alloc(meas->arena, name_len + 1, alignof(char));
            memcpy(meas->name, str.data, name_len);
            meas->name[name_len] = '\0';
            ARENA_PUSH(meas->arena, &meas->data.timestamp, int64_t, timestamp);
            ARENA_PUSH(meas->arena, &meas->data.value, float, val);
            ARENA_PUSH(ctx->temporaryArena, &ctx->measCache, Measurement, *meas);
        } else {
            bool found = false;
            for (size_t i = 0; i < ctx->measCache.count; i++) {
                Measurement* meas = &ctx->measCache.items[i];
                size_t meas_name_len = meas->name ? strlen(meas->name) : 0;
                if (meas_name_len == name_len && memcmp(str.data, meas->name, name_len) == 0) {
                    ARENA_PUSH(meas->arena, &meas->data.timestamp, int64_t, timestamp);
                    ARENA_PUSH(meas->arena, &meas->data.value, float, val);
                    log_info(
                        "MeasDataSize:%zu, capacity:%zu",
                        meas->data.timestamp.count,
                        meas->data.timestamp.capacity);
                    found = true;
                    break;
                }
            }
            if (!found) {
                log_info("Variable not found in cache creating one");
                Measurement* meas = measurement_create_in_arena(
                    ctx->temporaryArena, (char*)str.data, name_len, initialMeasSize);
                meas->name = (char*)arena_alloc(meas->arena, name_len + 1, alignof(char));
                memcpy(meas->name, str.data, name_len);
                meas->name[name_len] = '\0';
                ARENA_PUSH(meas->arena, &meas->data.timestamp, int64_t, timestamp);
                ARENA_PUSH(meas->arena, &meas->data.value, float, val);
                ARENA_PUSH(ctx->temporaryArena, &ctx->measCache, Measurement, *meas);
            }
        }
    }
    UA_String_clear(&str);
}
static int mirua_subscription_create(
    UA_Client* client, da_UA_NodeId* nodes, uint32_t* currentSubId) {
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
    *currentSubId = subId;

    for (size_t i = 0; i < nodes->count; i++) {
        UA_NodeId* node = &nodes->items[i];

        UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(*node);

        UA_MonitoredItemCreateResult monRet = UA_Client_MonitoredItems_createDataChange(
            client,
            subId,
            UA_TIMESTAMPSTORETURN_BOTH,
            monRequest,
            node,
            handler_TheAnswerChanged,
            NULL);
        if (monRet.statusCode == UA_STATUSCODE_GOOD) {
            log_trace("Subscription created, subId=%u\n", subId);
        } else {
            // TODO: add nodeid that was invalkid into logging.
            log_warn(
                "Failed to create monitored item: %s\n", UA_StatusCode_name(monRet.statusCode));
            return 0;
        }
    }
    // UA_MonitoredItemCreateRequest monRequest2 =
    // UA_MonitoredItemCreateRequest_default(second);
    //
    // UA_MonitoredItemCreateResult monRet2 = UA_Client_MonitoredItems_createDataChange(
    //     client,
    //     subId,
    //     UA_TIMESTAMPSTORETURN_BOTH,
    //     monRequest2,
    //     (void*)&second,
    //     handler_TheAnswerChanged,
    //     NULL);
    //
    // if (monRet2.statusCode == UA_STATUSCODE_GOOD) {
    //     log_trace("Subscription created, subId=%u\n", subId);
    // } else {
    //     log_error("Failed to create monitored item: %s\n",
    //     UA_StatusCode_name(monRet.statusCode)); return 0;
    // }
    return 1;
}
static void monCallback(
    UA_Client* client, void* userdata, UA_UInt32 requestId, UA_CreateMonitoredItemsResponse* r) {
    if (0 < r->resultsSize && r->results[0].statusCode == UA_STATUSCODE_GOOD) {
        UA_LOG_INFO(
            UA_Log_Stdout,
            UA_LOGCATEGORY_APPLICATION,
            "Monitoring UA_NS0ID_SERVER_SERVERSTATUS_CURRENTTIME', id %u",
            r->results[0].monitoredItemId);
    }
}
static void handler_currentTimeChanged(
    UA_Client* client,
    UA_UInt32 subId,
    void* subContext,
    UA_UInt32 monId,
    void* monContext,
    UA_DataValue* value) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION, "currentTime has changed!");
    if (UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_DATETIME])) {
        UA_DateTime raw_date = *(UA_DateTime*)value->value.data;
        UA_DateTimeStruct dts = UA_DateTime_toStruct(raw_date);
        UA_LOG_INFO(
            UA_Log_Stdout,
            UA_LOGCATEGORY_APPLICATION,
            "date is: %02u-%02u-%04u %02u:%02u:%02u.%03u",
            dts.day,
            dts.month,
            dts.year,
            dts.hour,
            dts.min,
            dts.sec,
            dts.milliSec);
    }
}

static void stateCallback(
    UA_Client* client,
    UA_SecureChannelState channelState,
    UA_SessionState sessionState,
    UA_StatusCode recoveryStatus) {
    uadb_context* ctx = (uadb_context*)UA_Client_getContext(client);
    switch (channelState) {
        case UA_SECURECHANNELSTATE_CLOSED:
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION, "The client is disconnected");
            break;
        case UA_SECURECHANNELSTATE_HEL_SENT:
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION, "Waiting for ack");
            break;
        case UA_SECURECHANNELSTATE_OPN_SENT:
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION, "Waiting for OPN Response");
            break;
        case UA_SECURECHANNELSTATE_OPEN:
            UA_LOG_INFO(
                UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION, "A SecureChannel to the server is open");
            break;
        default: break;
    }

    switch (sessionState) {
        case UA_SESSIONSTATE_ACTIVATED: {
            ctx->connected = true;
        } break;
        case UA_SESSIONSTATE_CLOSED:
            // log_warn("Session disconnected");
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION, "Session disconnected");
            break;
        default: break;
    }
}
typedef struct uadb_config {
    const char* configPath;
    const char* uaEndpoint;
    bool autoReconnect;
    memory_arena* temporaryArena;
    memory_arena* persistentArena;
} uadb_config;

static int uabd_recorder(uadb_config config) {
    // TODO: clean arena at some point. add persistent arena and temp arena that gets cleared
    // more often
    uadb_context ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.temporaryArena = config.temporaryArena;
    ctx.persistentArena = config.persistentArena;

    {
        DbContext dbCtx;
        dbCtx.handle = NULL;
        dbCtx.arena = ctx.persistentArena;
        dbCtx.dbName = arena_strdup(ctx.persistentArena, "default.db", alignof(char));
        dbCtx.dbSchemaFilename = arena_strdup(ctx.persistentArena, "schema.txt", alignof(char));
        ctx.db = dbCtx;
    }

    char* buf = fs_read_file(config.configPath, ctx.temporaryArena);
    if (!buf) {
        log_warn("[APP] - config read failed");
        return 0;
    }
    ctx.configLastTouch = fs_file_get_last_touch(config.configPath);

    Scanner scanner;
    lx_init(&scanner, buf, ctx.temporaryArena);
    arr_Tokens* tokens = lx_tokenize(&scanner);
    printf("%s", lx_tokensToStringArena(tokens, ctx.temporaryArena));

    Parser parser;
    parser_init(&parser, tokens, ctx.persistentArena);
    // TODO: repeats in reload. find a way to clear it. monitored items currently takes pointer to
    // the nodes so deleting them in temp
    // arena would cause problems
    ctx.nodes = parser_parse(&parser);

    for (size_t i = 0; i < ctx.nodes->count; i++) {
        UA_String str = {0};
        UA_NodeId_print(&ctx.nodes->items[i], &str);
        printf("%.*s\n", (int)str.length, str.data);
        UA_String_clear(&str);
    }

    if (!db_connect(&ctx.db)) {
        log_warn("DB Connect failed. Returning from uadb.");
        return 0;
    }

    arr_db_schema_variables* ptr = db_query_available_variables(ctx.db.handle, ctx.temporaryArena);

    if (ptr) {
        for (size_t i = 0; i < ptr->count; i++) {
            Db_schema_variable var = ptr->items[i];
            printf("Hello %s %zu %zu\n", var.name, var.id, var.unit_id);
        }
    }

    if (ctx.nodes->count != 0) {
        ctx.client = UA_Client_new();
        UA_ClientConfig* cc = UA_Client_getConfig(ctx.client);
        UA_ClientConfig_setDefault(cc);
        cc->clientContext = &ctx;
        cc->noReconnect = false;
        cc->connectivityCheckInterval = 1000;
        cc->stateCallback = stateCallback;

        UA_StatusCode retval = UA_Client_connect(ctx.client, config.uaEndpoint);
        if (retval != UA_STATUSCODE_GOOD) {
            log_warn("Test client failed to connect: %s", UA_StatusCode_name(retval));
        }

        static bool g_running = true;
        ctx.wasDisconnected = true;
        int64_t last_push = now_ms();
        const int64_t push_interval_ms = 5000;
        while (g_running) {
            UA_StatusCode st = UA_Client_run_iterate(ctx.client, 100);

            UA_SecureChannelState ch;
            UA_SessionState se;
            UA_StatusCode cs;
            UA_Client_getState(ctx.client, &ch, &se, &cs);
            if (ch == UA_SECURECHANNELSTATE_CLOSED && cs != UA_STATUSCODE_GOOD &&
                config.autoReconnect) {
                log_info("Trying to reconnect");
                ctx.wasDisconnected = true;
                UA_Client_connect(ctx.client, config.uaEndpoint);
            }

            if (st == UA_STATUSCODE_GOOD && ctx.wasDisconnected) {
                // TODO: Delete old subscriptions
                mirua_subscription_create(ctx.client, ctx.nodes, &ctx.currentSubId);
                arena_reset(ctx.temporaryArena, false);
                ctx.wasDisconnected = false;
            }

            int64_t cur = now_ms();
            if (cur - last_push >= push_interval_ms) {
                last_push = cur;
                if (ctx.measCache.count == 0) {
                    continue;
                }

                size_t count = 0;
                db_write_begin(ctx.db.handle);
                for (size_t i = 0; i < ctx.measCache.count; i++) {
                    Measurement* meas = &ctx.measCache.items[i];
                    if (meas->data.timestamp.count > 0) {
                        while (meas->data.timestamp.count != 0 && meas->data.value.count != 0) {
                            int64_t timestamp = nob_da_pop(&meas->data.timestamp);
                            float value = nob_da_pop(&meas->data.value);
                            UA_DateTimeStruct dts = UA_DateTime_toStruct(timestamp);
                            UA_Int64 epoch_ms =
                                UA_DateTime_toUnixTime(timestamp) * 1000LL + (UA_Int64)dts.milliSec;

                            db_write(ctx.db.handle, epoch_ms, meas->name, value);
                            count++;
                        }
                    }
                }
                db_write_end();
                log_trace("Wrote to database with %zu values", count);
            }

            if (ctx.monitoredDeleteQue.count != 0) {
                while (ctx.monitoredDeleteQue.count != 0) {
                    MonitoredItem monItem = nob_da_pop(&ctx.monitoredDeleteQue);
                    UA_Client_MonitoredItems_deleteSingle(ctx.client, monItem.subId, monItem.monId);
                    log_warn(
                        "Deleted monitoredItem with subId:%i, monId:%i",
                        monItem.subId,
                        monItem.monId);
                    // TODO: look into deleting multiple at once?
                    //  UA_Client_MonitoredItems_delete(UA_Client *client, const
                    //  UA_DeleteMonitoredItemsRequest)
                }
            }

            if (fs_file_has_changed(config.configPath, ctx.configLastTouch)) {
                log_info("Config file changed. Reloading config");

                char* buf = fs_read_file(config.configPath, ctx.temporaryArena);
                if (!buf) {
                    log_warn("[APP] - config read failed");
                    return 0;
                }
                ctx.configLastTouch = fs_file_get_last_touch(config.configPath);

                Scanner scanner;
                lx_init(&scanner, buf, ctx.temporaryArena);
                arr_Tokens* tokens = lx_tokenize(&scanner);

                printf("%s", lx_tokensToStringArena(tokens, ctx.temporaryArena));

                Parser parser;
                parser_init(&parser, tokens, ctx.persistentArena);
                ctx.nodes = parser_parse(&parser);

                if (UA_Client_Subscriptions_deleteSingle(ctx.client, ctx.currentSubId) ==
                    UA_STATUSCODE_GOOD) {
                    log_trace("Deleting old subcription was succesfull");
                } else {
                    NOB_ASSERT("Deleting subscription failed.");
                };

                mirua_subscription_create(ctx.client, ctx.nodes, &ctx.currentSubId);
            }
        }

        UA_Client_disconnect(ctx.client);
        UA_Client_delete(ctx.client);
    }
    return 1;
}

/* Callback for state changes. The client state is differentated into the
 * SecureChannel state and the Session state. The connectStatus is set if
 * the client connection (including reconnects) has failed and the client
 * has to "give up". If the connectStatus is not set, the client still has
 * hope to connect or recover. */
int main(int argc, char* argv[]) {
    memory_arena* persistentArena = arena_create(KB(16));
    memory_arena* tempArena = arena_create(MB(1));

    uadb_config config = {
        .configPath = "./output.txt",
        .uaEndpoint = "opc.tcp://localhost:4840",
        .autoReconnect = true,
        .persistentArena = persistentArena,
        .temporaryArena = tempArena};
    int st = uabd_recorder(config);

    return 0;
}
