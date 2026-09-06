

#include <linux/limits.h>
#include <open62541/plugin/log_stdout.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "core/allocator.h"
#include "core/fileio.h"
#include "core/log.h"
#include "core/nob.h"
#include "core/string.h"
#include "core/types.h"
#include "include/db_access.h"
#include "lexer.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/client_subscriptions.h"
#include "open62541/types.h"
#include "parser.h"

typedef struct uadb_context {
    // char* executableDir;
    String_Builder executableDir;
    // const char* dbDir;
    String_Builder schemaPath;
    // const char* configDir;
    String_Builder dbPath;
    String_Builder opcuaConfigPath;
    UA_Client* client;
    da_UA_NodeId* nodes;
    bool connected;
    bool wasDisconnected;
    bool pushBuffer;
    arr_Measurements measCache;
    memory_arena* persistentArena;
    memory_arena* temporaryArena;
    memory_arena* configArena;
    arr_MonitoredItem monitoredDeleteQue;
    arr_MonitoredItem monitoredItems;
    uint32_t currentSubId;
    time_t configLastTouch;
    DbContext db;
} uadb_context;

static void handler_TheAnswerChanged(
    UA_Client* client,
    UA_UInt32 subId,
    void* subContext,
    UA_UInt32 monId,
    void* monContext,
    UA_DataValue* value) {
    uadb_context* ctx = (uadb_context*)UA_Client_getContext(client);

    float val = -999999.0f;
    int64_t timestamp = -1;

    MonitoredItem item = {0};
    bool foundMonItem = false;
    for (size_t i = 0; i < ctx->monitoredItems.count; i++) {
        MonitoredItem cur = ctx->monitoredItems.items[i];
        if (cur.monId == monId && cur.subId == subId) {
            item = ctx->monitoredItems.items[i];
            foundMonItem = true;
        }
    }
    size_t initialMeasSize = 128;
    if (foundMonItem == false) {
        log_error("this probably shoudln't happen. check whats going on here lol....");
        return;
    }

    if (value->hasValue && value->serverTimestamp && UA_Variant_isScalar(&value->value)) {
        // TODO: test the different types. downcasting
        const UA_DataType* type = value->value.type;
        if (type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
            val = *(UA_Boolean*)value->value.data ? 1.0 : 0.0;
        } else if (type == &UA_TYPES[UA_TYPES_SBYTE]) {
            val = (float)*(UA_SByte*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_BYTE]) {
            val = (float)*(UA_Byte*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_INT16]) {
            val = (float)*(UA_Int16*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_UINT16]) {
            val = (float)*(UA_UInt16*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_INT32]) {
            val = (float)*(UA_Int32*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_UINT32]) {
            val = (float)*(UA_UInt32*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_INT64]) {
            val = (float)*(UA_Int64*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_UINT64]) {
            val = (float)*(UA_UInt64*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_FLOAT]) {
            val = (float)*(UA_Float*)value->value.data;
        } else if (type == &UA_TYPES[UA_TYPES_DOUBLE]) {
            // TODO: is this correct?
            val = *(float*)value->value.data;
        } else {
            // TODO: Why am i doing this? why not use item straight away?
            MonitoredItem monItem = {.subId = subId, .monId = monId, .name = item.name};
            for (size_t i = 0; i < ctx->monitoredDeleteQue.count; i++) {
                monItem = ctx->monitoredDeleteQue.items[i];

                if (monItem.monId == monId && monItem.subId == subId) {
                    return;
                }
            }
            mir_da_arena_append(
                ctx->temporaryArena, &ctx->monitoredDeleteQue, MonitoredItem, monItem);

            log_warn(
                "Subscripted to not supported variable type, scheduled removal (sub=%u, mon=%u).",
                subId,
                monId);
            return;
        }
        timestamp = value->serverTimestamp;

        if (ctx->measCache.count == 0) {
            Measurement* meas = measurement_create_in_arena(
                ctx->temporaryArena, item.name.items, item.name.count, initialMeasSize);
            mir_da_arena_append(meas->arena, &meas->data.timestamp, int64_t, timestamp);
            mir_da_arena_append(meas->arena, &meas->data.value, float, val);
            mir_da_arena_append(ctx->temporaryArena, &ctx->measCache, Measurement, *meas);
        } else {
            bool found = false;
            // TODO: if this bottlenecks convert code to hashmap.
            for (size_t i = 0; i < ctx->measCache.count; i++) {
                Measurement* meas = &ctx->measCache.items[i];
                size_t meas_name_len = meas->name ? strlen(meas->name) : 0;
                if (meas_name_len == item.name.count &&
                    memcmp(item.name.items, meas->name, item.name.count) == 0) {
                    mir_da_arena_append(meas->arena, &meas->data.timestamp, int64_t, timestamp);
                    mir_da_arena_append(meas->arena, &meas->data.value, float, val);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // log_info("Variable not found in cache creating one");
                Measurement* meas = measurement_create_in_arena(
                    ctx->temporaryArena, item.name.items, item.name.count, initialMeasSize);
                mir_da_arena_append(meas->arena, &meas->data.timestamp, int64_t, timestamp);
                mir_da_arena_append(meas->arena, &meas->data.value, float, val);
                mir_da_arena_append(ctx->temporaryArena, &ctx->measCache, Measurement, *meas);
            }
        }
        // UA_String_clear(&str);
    }
}
static int mirua_subscription_create(
    UA_Client* client, da_UA_NodeId* nodes, uint32_t* currentSubId) {
    uadb_context* ctx = (uadb_context*)UA_Client_getContext(client);

    UA_CreateSubscriptionRequest subRequest = UA_CreateSubscriptionRequest_default();
    UA_CreateSubscriptionResponse subResponse =
        UA_Client_Subscriptions_create(client, subRequest, NULL, NULL, NULL);
    if (subResponse.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        log_error(
            "Failed to create subscription: %s",
            UA_StatusCode_name(subResponse.responseHeader.serviceResult));
        return 0;
    }
    UA_UInt32 subId = subResponse.subscriptionId;
    *currentSubId = subId;

    // Build all requests at once
    arena_set_reset_point_current(ctx->temporaryArena);
    UA_MonitoredItemCreateRequest* monRequests = arena_alloc(
        ctx->temporaryArena,
        nodes->count * sizeof(UA_MonitoredItemCreateRequest),
        alignof(UA_MonitoredItemCreateRequest));
    UA_Client_DataChangeNotificationCallback* callbacks = arena_alloc(
        ctx->temporaryArena,
        nodes->count * sizeof(UA_Client_DataChangeNotificationCallback),
        alignof(void*));
    void** contexts_arr =
        arena_alloc(ctx->temporaryArena, nodes->count * sizeof(void*), alignof(void*));

    for (size_t i = 0; i < nodes->count; i++) {
        monRequests[i] = UA_MonitoredItemCreateRequest_default(nodes->items[i]);
        callbacks[i] = handler_TheAnswerChanged;
        contexts_arr[i] = NULL;
    }

    UA_CreateMonitoredItemsRequest req;
    UA_CreateMonitoredItemsRequest_init(&req);
    req.subscriptionId = subId;
    req.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
    req.itemsToCreate = monRequests;
    req.itemsToCreateSize = nodes->count;

    UA_CreateMonitoredItemsResponse resp =
        UA_Client_MonitoredItems_createDataChanges(client, req, contexts_arr, callbacks, NULL);
    arena_reset(ctx->temporaryArena, false);

    for (size_t i = 0; i < resp.resultsSize; i++) {
        if (resp.results[i].statusCode == UA_STATUSCODE_GOOD) {
            // log_trace("MonitoredItem created, subId=%u", subId);
            MonitoredItem monItem = {
                .nodeIdx = (uint32_t)i, .subId = subId, .monId = resp.results[i].monitoredItemId};
            sb_arena_append_buf(
                ctx->temporaryArena,
                &monItem.name,
                ctx->nodes->items[i].identifier.string.data,
                ctx->nodes->items[i].identifier.string.length,
                char);
            mir_da_arena_append(ctx->temporaryArena, &ctx->monitoredItems, MonitoredItem, monItem);
        } else {
            log_warn(
                "Failed to create monitored item: %s",
                UA_StatusCode_name(resp.results[i].statusCode));
        }
    }

    UA_CreateMonitoredItemsResponse_clear(&resp);
    return 1;
}

static size_t meascache_flush(DbContext* db, arr_Measurements* measCache) {
    if (!db || !measCache || measCache->count == 0) return 0;

    size_t written = 0;
    db_write_begin(db);
    for (size_t i = 0; i < measCache->count; ++i) {
        Measurement* meas = &measCache->items[i];
        size_t pairs = (meas->data.timestamp.count < meas->data.value.count)
            ? meas->data.timestamp.count
            : meas->data.value.count;
        for (; pairs; --pairs) {
            int64_t timestamp = nob_da_pop(&meas->data.timestamp);
            float value = nob_da_pop(&meas->data.value);
            UA_DateTimeStruct dts = UA_DateTime_toStruct(timestamp);
            UA_Int64 epoch_ms = UA_DateTime_toUnixTime(timestamp) * 1000LL + (UA_Int64)dts.milliSec;
            db_write(db, epoch_ms, meas->name, value);
            written++;
        }
    }
    db_write_end(db);
    return written;
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
    const char* opcuaConfigFilename;
    const char* dbName;
    const char* uaEndpoint;
    bool autoReconnect;
    memory_arena* temporaryArena;
    memory_arena* persistentArena;
} uadb_config;

static int uabd_recorder(uadb_config config) {
    // TODO: clean arena at some point. add persistent arena and temp arena that gets cleared
    // more often

    const char* dataDir = "/../data";
    // ===================================== INIT =====================================
    uadb_context ctx;
    memset(&ctx, 0, sizeof(ctx));

    ctx.temporaryArena = config.temporaryArena;
    ctx.persistentArena = config.persistentArena;
    ctx.configArena = arena_create(MB(1));

    int retVal = fs_sb_get_executable_dir(ctx.persistentArena, &ctx.executableDir);
    if (retVal == -1) {
        log_error("getting executable dir failed. Memory issue. Shouldn't happen ever");
    }

    sb_arena_append_buf(
        ctx.persistentArena, &ctx.dbPath, ctx.executableDir.items, ctx.executableDir.count, char);
    sb_arena_append_cstr(ctx.persistentArena, &ctx.dbPath, dataDir);
    sb_arena_append_cstr(ctx.persistentArena, &ctx.dbPath, config.dbName);
    sb_arena_append_null(ctx.persistentArena, &ctx.dbPath);

    sb_arena_append_buf(
        ctx.persistentArena,
        &ctx.schemaPath,
        ctx.executableDir.items,
        ctx.executableDir.count,
        char);
    sb_arena_append_cstr(ctx.persistentArena, &ctx.schemaPath, dataDir);
    sb_arena_append_cstr(ctx.persistentArena, &ctx.schemaPath, "/schema.txt");
    sb_arena_append_null(ctx.persistentArena, &ctx.schemaPath);

    // TODO: user might give path without /. validate input??
    sb_arena_append_buf(
        ctx.persistentArena,
        &ctx.opcuaConfigPath,
        ctx.executableDir.items,
        ctx.executableDir.count,
        char);
    sb_arena_append_cstr(ctx.persistentArena, &ctx.opcuaConfigPath, dataDir);
    sb_arena_append_cstr(ctx.persistentArena, &ctx.opcuaConfigPath, config.opcuaConfigFilename);
    sb_arena_append_null(ctx.persistentArena, &ctx.opcuaConfigPath);

    // ================================= SQLITE3 INIT =================================
    // TODO: make them accept sb? or make it accept SV since it doesnt edit it.?
    char* schema = db_read_sql_schema(ctx.schemaPath.items, ctx.temporaryArena);
    if (schema == NULL) {
        log_fatal(
            "[UADB] - Reading schema at %.*s failed. Returning from uadb.",
            (int)ctx.schemaPath.count,
            ctx.schemaPath.items);
        return 0;
    }

    if (!db_connect(&ctx.db, ctx.dbPath.items, schema)) {
        log_warn("[UADB] - DB Connect failed. Returning from uadb.");
        return 0;
    }

    // ================================= CONFIG PARSE =================================
    char* configBuf = fs_read_file(ctx.opcuaConfigPath.items, ctx.temporaryArena);
    if (configBuf) {
        ctx.configLastTouch = fs_file_get_last_touch(ctx.opcuaConfigPath.items);
        ctx.nodes = parser_parse(ctx.configArena, configBuf);

    } else {
        log_error("[UADB] - config read failed. Trying again in 1s. ");
        bool valid = false;
        while (!valid) {
            log_info(
                "Memory - Persistent:count%zu,size%zu - temporary:count%zu,size%zu",
                ctx.persistentArena->offset,
                ctx.persistentArena->size,
                ctx.temporaryArena->offset,
                ctx.temporaryArena->size);
            sleep(1);
            if (fs_file_has_changed(ctx.opcuaConfigPath.items, ctx.configLastTouch)) {
                log_info("Config file changed. Reloading config");
                configBuf = fs_read_file(ctx.opcuaConfigPath.items, ctx.temporaryArena);
                if (!configBuf) {
                    log_error("[UADB] - config read failed. Trying again in 1s. ");
                    continue;
                }
                ctx.configLastTouch = fs_file_get_last_touch(ctx.opcuaConfigPath.items);
                ctx.nodes = parser_parse(ctx.configArena, configBuf);
                if (ctx.nodes->count <= 0) {
                    log_error("[UADB] - config doesn't contain any nodes. Trying again in 1s. ");
                    continue;
                } else {
                    valid = true;
                }
            } else {
                log_info("Waiting for config:%s to be modified", ctx.opcuaConfigPath.items);
            }
        }
    }

    // ================================= OPC UA INIT ==================================
    ctx.client = UA_Client_new();
    UA_ClientConfig* cc = UA_Client_getConfig(ctx.client);
    UA_ClientConfig_setDefault(cc);
    cc->clientContext = &ctx;
    cc->noReconnect = false;
    cc->connectivityCheckInterval = 1000;
    cc->stateCallback = stateCallback;

    UA_StatusCode retval = UA_STATUSCODE_BAD;
    uint retryingTime = 1;
    while ((retval = UA_Client_connect(ctx.client, config.uaEndpoint)) != UA_STATUSCODE_GOOD) {
        log_warn(
            "Failed to connect to %s: %s. Retrying in %u s.",
            config.uaEndpoint,
            UA_StatusCode_name(retval),
            retryingTime);
        sleep(retryingTime);
    }

    // ================================== MAIN LOOP ===================================
    static bool g_running = true;
    ctx.wasDisconnected = true;
    int64_t last_push = now_ms();
    const int64_t push_interval_ms = 5000;
    arena_reset(ctx.temporaryArena, false);
    while (g_running) {
        // log_info(
        //     "Memory - Persistent:count%zu,size%zu - temporary:count%zu,size%zu",
        //     ctx.persistentArena->offset,
        //     ctx.persistentArena->size,
        //     ctx.temporaryArena->offset,
        //     ctx.temporaryArena->size);
        //
        UA_StatusCode st = UA_Client_run_iterate(ctx.client, 100);

        // ============================= CHECKING CONNECTION ==============================
        // TODO: instead of checking it use callback.
        UA_SecureChannelState ch;
        UA_SessionState se;
        UA_StatusCode cs;
        UA_Client_getState(ctx.client, &ch, &se, &cs);
        if (ch == UA_SECURECHANNELSTATE_CLOSED && cs != UA_STATUSCODE_GOOD &&
            config.autoReconnect) {
            log_info("Connection was closed. Trying to reconnect");
            ctx.wasDisconnected = true;
            UA_Client_connect(ctx.client, config.uaEndpoint);
        }

        // ============================= CREATE SUBSCRIPTIONS ==============================
        if (st == UA_STATUSCODE_GOOD && ctx.wasDisconnected) {
            // flush measCache to prevent datalos.

            size_t written = meascache_flush(&ctx.db, &ctx.measCache);
            if (written != 0) {
                log_info(
                    "OpcUa was disconnected. flushed measCache and wrote %zu values into db.",
                    written);
            }

            // TODO: if this becomes used elsewhere make it into function so i dont forget to memset
            // the stuff.
            arena_reset(ctx.temporaryArena, false);
            memset(&ctx.measCache, 0, sizeof(ctx.measCache));
            memset(&ctx.monitoredItems, 0, sizeof(ctx.monitoredItems));
            memset(&ctx.monitoredDeleteQue, 0, sizeof(ctx.monitoredDeleteQue));
            mirua_subscription_create(ctx.client, ctx.nodes, &ctx.currentSubId);
            ctx.wasDisconnected = false;
        }

        // ============================= WRITING TO DATABASE ==============================
        int64_t cur = now_ms();
        if (cur - last_push >= push_interval_ms) {
            log_info(
                "Memory - Persistent:count%zu,size%zu - temporary:count%zu,size%zu, "
                "config:%zu,size%zu",
                ctx.persistentArena->offset,
                ctx.persistentArena->size,
                ctx.temporaryArena->offset,
                ctx.temporaryArena->size,
                ctx.configArena->offset,
                ctx.configArena->size);

            last_push = cur;

            size_t total_pairs = 0;
            for (size_t i = 0; i < ctx.measCache.count; i++) {
                Measurement* m = &ctx.measCache.items[i];
                total_pairs += (m->data.timestamp.count < m->data.value.count)
                    ? m->data.timestamp.count
                    : m->data.value.count;
                if (total_pairs > 0) {
                    break;
                }
            }

            if (total_pairs == 0) {
            } else {
                size_t written = meascache_flush(&ctx.db, &ctx.measCache);
                log_info("Wrote %zu values to db.", written);
            }
        }

        // ======================== DELETING UNUSED MONITOREDITEMS ========================
        if (ctx.monitoredDeleteQue.count != 0) {
            log_info(
                "cacheSize:%zu monitoredItemsSize:%zu deleteSize%zu",
                ctx.measCache.count,
                ctx.monitoredItems.count,
                ctx.monitoredDeleteQue.count);

            while (ctx.monitoredDeleteQue.count != 0) {
                MonitoredItem monItem = nob_da_pop(&ctx.monitoredDeleteQue);
                UA_Client_MonitoredItems_deleteSingle(ctx.client, monItem.subId, monItem.monId);
                log_warn(
                    "Deleted monitoredItem with subId:%i, monId:%i, name:\"%s\"",
                    monItem.subId,
                    monItem.monId,
                    monItem.name.items);
                // TODO: use hashmap :))
                for (size_t i = 0; i < ctx.monitoredItems.count; i++) {
                    MonitoredItem* item = &ctx.monitoredItems.items[i];
                    if (item->monId == monItem.monId && item->subId == monItem.subId) {
                        nob_da_remove_unordered(&ctx.monitoredItems, i);
                        break;
                    }
                }
                // TODO: look into deleting multiple at once?
                //  UA_Client_MonitoredItems_delete(UA_Client *client, const
                //  UA_DeleteMonitoredItemsRequest)
            }
        }

        // ================================ CONFIG RELOAD =================================
        if (fs_file_has_changed(ctx.opcuaConfigPath.items, ctx.configLastTouch)) {
            log_info("Config file changed. Reloading config");
            arena_reset(ctx.temporaryArena, false);
            arena_reset(ctx.configArena, false);
            memset(&ctx.measCache, 0, sizeof(ctx.measCache));
            memset(&ctx.monitoredItems, 0, sizeof(ctx.monitoredItems));
            memset(&ctx.monitoredDeleteQue, 0, sizeof(ctx.monitoredDeleteQue));

            char* buf = fs_read_file(ctx.opcuaConfigPath.items, ctx.temporaryArena);
            if (!buf) {
                log_warn("[APP] - config read failed");
                return 0;
            }
            ctx.configLastTouch = fs_file_get_last_touch(ctx.opcuaConfigPath.items);
            ctx.nodes = parser_parse(ctx.configArena, buf);

            if (UA_Client_Subscriptions_deleteSingle(ctx.client, ctx.currentSubId) ==
                UA_STATUSCODE_GOOD) {
                log_trace("Deleting old subcription was succesfull");
            } else {
                log_warn("Deleting subscription failed.");
            }

            mirua_subscription_create(ctx.client, ctx.nodes, &ctx.currentSubId);
        }
    }

    // =================================== CLEANUP ====================================
    UA_Client_disconnect(ctx.client);
    UA_Client_delete(ctx.client);

    return 1;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    memory_arena* persistentArena = arena_create(KB(128));
    memory_arena* tempArena = arena_create(MB(1));

    uadb_config config = {
        .opcuaConfigFilename = "/output.txt",
        .uaEndpoint = "opc.tcp://localhost:4840",
        .dbName = "/firstDb.db",

        .autoReconnect = true,
        .persistentArena = persistentArena,
        .temporaryArena = tempArena};
    int st = uabd_recorder(config);

    return 0;
}
