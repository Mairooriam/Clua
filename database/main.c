

#include <linux/limits.h>
#include <open62541/plugin/log_stdout.h>
#include <stdalign.h>
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
#include "include/db_access.h"
#include "lexer.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/client_subscriptions.h"
#include "open62541/types.h"
#include "parser.h"

typedef struct uadb_context {
    String_Builder executableDir;
    // char* executableDir;
    const char* dbDir;
    const char* configDir;
    UA_Client* client;
    da_UA_NodeId* nodes;
    bool connected;
    bool wasDisconnected;
    bool pushBuffer;
    arr_Measurements measCache;
    memory_arena* temporaryArena;
    memory_arena* persistentArena;
    arr_MonitoredItem monitoredDeleteQue;
    arr_MonitoredItem monitoredItems;
    uint32_t currentSubId;
    time_t configLastTouch;
    char* dbName;
    char* dbSchemaFilename;
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
            MonitoredItem monItem = {.subId = subId, .monId = monId};
            for (size_t i = 0; i < ctx->monitoredDeleteQue.count; i++) {
                monItem = ctx->monitoredDeleteQue.items[i];

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

        // log_trace(
        //     "callback from %u node:%.*s with value of type:%s, cacheSize:%zu",
        //     subId,
        //     (int)str.length,
        //     str.data,
        //     value->value.type->typeName,
        //     ctx->measCache.count);

        UA_String str = {0};
        uint32_t idx = 0;
        for (size_t i = 0; i < ctx->monitoredItems.count; i++) {
            MonitoredItem item = ctx->monitoredItems.items[i];
            if (item.monId == monId && item.subId == subId) {
                idx = item.nodeIdx;
            }
        }
        UA_NodeId_print(&ctx->nodes->items[idx], &str);

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
                    // log_info(
                    //     "MeasDataSize:%zu, capacity:%zu",
                    //     meas->data.timestamp.count,
                    //     meas->data.timestamp.capacity);
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
        UA_String_clear(&str);
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

    for (size_t i = 0; i < nodes->count; i++) {
        // TODO: make some struct for monitoreditems to keep track of current subscriptions?
        UA_NodeId* node = &nodes->items[i];

        UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(*node);

        UA_MonitoredItemCreateResult monRet = UA_Client_MonitoredItems_createDataChange(
            client,
            subId,
            UA_TIMESTAMPSTORETURN_BOTH,
            monRequest,
            NULL,
            handler_TheAnswerChanged,
            NULL);
        if (monRet.statusCode == UA_STATUSCODE_GOOD) {
            log_trace("MonitoredItem created, subId=%u", subId);
        } else {
            // TODO: add nodeid that was invalkid into logging.
            log_warn("Failed to create monitored item: %s", UA_StatusCode_name(monRet.statusCode));
            continue;
        }

        MonitoredItem monItem = (MonitoredItem){
            .nodeIdx = (uint32_t)i, .subId = subId, .monId = monRet.monitoredItemId};
        ARENA_PUSH(ctx->temporaryArena, &ctx->monitoredItems, MonitoredItem, monItem);
    }
    return 1;
}
static int mirua_subscription_create2(
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

    size_t count = nodes->count;
    UA_MonitoredItemCreateRequest* monRequests =
        malloc(count * sizeof(UA_MonitoredItemCreateRequest));
    UA_Client_DataChangeNotificationCallback* callbacks =
        malloc(count * sizeof(UA_Client_DataChangeNotificationCallback));
    void** contexts_arr = malloc(count * sizeof(void*));

    if (!monRequests || !callbacks || !contexts_arr) {
        free(monRequests);
        free(callbacks);
        free(contexts_arr);
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        monRequests[i] = UA_MonitoredItemCreateRequest_default(nodes->items[i]);
        callbacks[i] = handler_TheAnswerChanged;
        contexts_arr[i] = NULL;
    }

    UA_CreateMonitoredItemsRequest req;
    UA_CreateMonitoredItemsRequest_init(&req);
    req.subscriptionId = subId;
    req.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
    req.itemsToCreate = monRequests;
    req.itemsToCreateSize = count;

    UA_CreateMonitoredItemsResponse resp =
        UA_Client_MonitoredItems_createDataChanges(client, req, contexts_arr, callbacks, NULL);

    for (size_t i = 0; i < resp.resultsSize; i++) {
        if (resp.results[i].statusCode == UA_STATUSCODE_GOOD) {
            log_trace("MonitoredItem created, subId=%u", subId);
            MonitoredItem monItem = {
                .nodeIdx = (uint32_t)i, .subId = subId, .monId = resp.results[i].monitoredItemId};
            ARENA_PUSH(ctx->temporaryArena, &ctx->monitoredItems, MonitoredItem, monItem);
        } else {
            log_warn(
                "Failed to create monitored item: %s",
                UA_StatusCode_name(resp.results[i].statusCode));
        }
    }

    UA_CreateMonitoredItemsResponse_clear(&resp);
    free(monRequests);
    free(callbacks);
    free(contexts_arr);
    return 1;
}
static int mirua_subscription_create3(
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

    for (size_t i = 0; i < resp.resultsSize; i++) {
        if (resp.results[i].statusCode == UA_STATUSCODE_GOOD) {
            log_trace("MonitoredItem created, subId=%u", subId);
            MonitoredItem monItem = {
                .nodeIdx = (uint32_t)i, .subId = subId, .monId = resp.results[i].monitoredItemId};
            ARENA_PUSH(ctx->temporaryArena, &ctx->monitoredItems, MonitoredItem, monItem);
        } else {
            log_warn(
                "Failed to create monitored item: %s",
                UA_StatusCode_name(resp.results[i].statusCode));
        }
    }

    UA_CreateMonitoredItemsResponse_clear(&resp);
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
    const char* dbName;
    const char* uaEndpoint;
    bool autoReconnect;
    memory_arena* temporaryArena;
    memory_arena* persistentArena;
} uadb_config;

static int uabd_recorder(uadb_config config) {
    // TODO: clean arena at some point. add persistent arena and temp arena that gets cleared
    // more often

    // ===================================== INIT =====================================
    uadb_context ctx;
    memset(&ctx, 0, sizeof(ctx));

    // ctx.executableDir =
    //     (String_Builder*)arena_alloc(ctx.persistentArena, PATH_MAX, alignof(String_Builder));
    // PATH_MAX - 1 to fit null byte
    // int retVal = fs_get_executable_dir(ctx.executableDir, PATH_MAX - 1);
    // if (retVal == -1) {
    //     log_error("getting executable dir failed. Memory issue. Shouldn't happen ever");
    // }
    ctx.configDir = "/data/";
    ctx.dbDir = "/data/";

    String_Builder sb;
    // sb_append

    ctx.temporaryArena = config.temporaryArena;
    ctx.persistentArena = config.persistentArena;
    ctx.dbName = arena_strdup(ctx.temporaryArena, config.dbName, alignof(char));
    ctx.dbSchemaFilename = arena_strdup(ctx.temporaryArena, "schema.txt", alignof(char));
    db_context_init(&ctx.db);

    // ================================= SQLITE3 INIT =================================
    char* schema = db_read_sql_schema(ctx.dbSchemaFilename, ctx.temporaryArena);
    if (!db_connect(&ctx.db, ctx.dbName, schema)) {
        log_warn("DB Connect failed. Returning from uadb.");
        return 0;
    }

    // ================================= CONFIG PARSE =================================
    char* buf = fs_read_file(config.configPath, ctx.temporaryArena);
    if (!buf) {
        log_warn("[APP] - config read failed");
        return 0;
    }
    ctx.configLastTouch = fs_file_get_last_touch(config.configPath);
    Scanner scanner;
    lx_init(&scanner, buf, ctx.temporaryArena);
    arr_Tokens* tokens = lx_tokenize(&scanner);
    Parser parser;
    parser_init(&parser, tokens, ctx.temporaryArena);
    // TODO: repeats in reload. find a way to clear it. monitored items currently takes pointer to
    // the nodes so deleting them in temp
    // arena would cause problems
    ctx.nodes = parser_parse(&parser);

    // =========================== INVALID CONFIG FALLBACK ============================
    if (ctx.nodes->count == 0) {
        log_warn("Config didn't contain any nodes. Update config with correct one!");
        while (ctx.nodes->count == 0) {
            sleep(1);
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
                Parser parser;
                parser_init(&parser, tokens, ctx.temporaryArena);
                // TODO: repeats in reload. find a way to clear it. monitored items currently takes
                // pointer to the nodes so deleting them in temp arena would cause problems
                ctx.nodes = parser_parse(&parser);
            } else {
                log_info("Waiting for config:%s to be modified", config.configPath);
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

    UA_StatusCode retval = UA_Client_connect(ctx.client, config.uaEndpoint);
    if (retval != UA_STATUSCODE_GOOD) {
        log_warn("Test client failed to connect: %s", UA_StatusCode_name(retval));
    }

    // ================================== MAIN LOOP ===================================
    static bool g_running = true;
    ctx.wasDisconnected = true;
    int64_t last_push = now_ms();
    const int64_t push_interval_ms = 5000;
    while (g_running) {
        log_info(
            "Memory - Persistent:count%zu,size%zu - temporary:count%zu,size%zu",
            ctx.persistentArena->offset,
            ctx.persistentArena->size,
            ctx.temporaryArena->offset,
            ctx.temporaryArena->size);

        UA_StatusCode st = UA_Client_run_iterate(ctx.client, 100);

        // ============================= CHECKING CONNECTION ==============================
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

        // ============================= WRITING TO DATABASE ==============================
        int64_t cur = now_ms();
        if (cur - last_push >= push_interval_ms) {
            last_push = cur;
            if (ctx.measCache.count == 0) {
                continue;
            }

            size_t count = 0;
            db_write_begin(&ctx.db);
            for (size_t i = 0; i < ctx.measCache.count; i++) {
                Measurement* meas = &ctx.measCache.items[i];
                log_info(
                    "Meas name:%s cacheSize:%zu monitoredItemsSize:%zu deleteSize%zu",
                    meas->name,
                    ctx.measCache.count,
                    ctx.monitoredItems.count,
                    ctx.monitoredDeleteQue.count);
                if (meas->data.timestamp.count > 0) {
                    size_t pairs = (meas->data.timestamp.count < meas->data.value.count)
                        ? meas->data.timestamp.count
                        : meas->data.value.count;
                    for (; pairs; --pairs) {
                        int64_t timestamp = nob_da_pop(&meas->data.timestamp);
                        float value = nob_da_pop(&meas->data.value);
                        UA_DateTimeStruct dts = UA_DateTime_toStruct(timestamp);
                        UA_Int64 epoch_ms =
                            UA_DateTime_toUnixTime(timestamp) * 1000LL + (UA_Int64)dts.milliSec;

                        db_write(&ctx.db, epoch_ms, meas->name, value);
                        count++;
                    }
                }
            }
            db_write_end(&ctx.db);
            log_trace("Wrote to database with %zu values", count);
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
                    "Deleted monitoredItem with subId:%i, monId:%i", monItem.subId, monItem.monId);
                // TODO: use hashmap :))
                for (size_t i = 0; i < ctx.monitoredItems.count; i++) {
                    MonitoredItem* item = &ctx.monitoredItems.items[i];
                    if (item->monId == monItem.monId && item->subId == monItem.subId) {
                        log_info("Removed deleted monitor from monitores");
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
        if (fs_file_has_changed(config.configPath, ctx.configLastTouch)) {
            log_info("Config file changed. Reloading config");
            arena_reset(ctx.temporaryArena, false);
            memset(&ctx.measCache, 0, sizeof(ctx.measCache));

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
            parser_init(&parser, tokens, ctx.temporaryArena);
            ctx.nodes = parser_parse(&parser);

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

    memory_arena* persistentArena = arena_create(KB(1));
    memory_arena* tempArena = arena_create(MB(1));

    uadb_config config = {
        .configPath = "./output.txt",
        .uaEndpoint = "opc.tcp://localhost:4840",
        .dbName = "firstDb.db",

        .autoReconnect = true,
        .persistentArena = persistentArena,
        .temporaryArena = tempArena};
    int st = uabd_recorder(config);

    return 0;
}
