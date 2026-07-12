

#include <open62541/plugin/log_stdout.h>
#include <stdint.h>
#include <stdio.h>

#include "core/allocator.h"
#include "include/db_access.h"
#include "lexer.h"
#include "log.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/client_highlevel.h"
#include "open62541/client_subscriptions.h"
#include "open62541/types.h"
#include "parser.h"

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
static void subscriptionInactivityCallback(UA_Client* client, UA_UInt32 subId, void* subContext) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION, "Inactivity for subscription %u", subId);
}

static void deleteSubscriptionCallback(
    UA_Client* client, UA_UInt32 subscriptionId, void* subscriptionContext) {
    UA_LOG_INFO(
        UA_Log_Stdout,
        UA_LOGCATEGORY_APPLICATION,
        "Subscription Id %u was deleted",
        subscriptionId);
}
static void createSubscriptionCallback(
    UA_Client* client, void* userdata, UA_UInt32 requestId, UA_CreateSubscriptionResponse* r) {
    if (r->subscriptionId == 0) {
        UA_LOG_ERROR(
            UA_Log_Stdout,
            UA_LOGCATEGORY_APPLICATION,
            "response->subscriptionId == 0, %u",
            r->subscriptionId);
    } else if (r->responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_LOG_INFO(
            UA_Log_Stdout,
            UA_LOGCATEGORY_APPLICATION,
            "Create subscription failed, serviceResult %u",
            r->responseHeader.serviceResult);
    } else {
        UA_LOG_INFO(
            UA_Log_Stdout,
            UA_LOGCATEGORY_APPLICATION,
            "Create subscription succeeded, id %u",
            r->subscriptionId);

        /* Add a MonitoredItem */
        UA_NodeId currentTime = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERSTATUS_CURRENTTIME);
        UA_CreateMonitoredItemsRequest req;
        UA_CreateMonitoredItemsRequest_init(&req);
        UA_MonitoredItemCreateRequest monRequest =
            UA_MonitoredItemCreateRequest_default(currentTime);
        req.itemsToCreate = &monRequest;
        req.itemsToCreateSize = 1;
        req.subscriptionId = r->subscriptionId;

        UA_Client_DataChangeNotificationCallback dataChangeNotificationCallback[1] = {
            handler_currentTimeChanged};
        UA_StatusCode retval = UA_Client_MonitoredItems_createDataChanges_async(
            client, req, NULL, dataChangeNotificationCallback, NULL, monCallback, NULL, NULL);
        if (retval != UA_STATUSCODE_GOOD)
            UA_LOG_ERROR(
                UA_Log_Stdout,
                UA_LOGCATEGORY_APPLICATION,
                "UA_Client_MonitoredItems_createDataChanges_async ",
                UA_StatusCode_name(retval));
    }
}

static void stateCallback(
    UA_Client* client,
    UA_SecureChannelState channelState,
    UA_SessionState sessionState,
    UA_StatusCode recoveryStatus) {
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
            UA_LOG_INFO(
                UA_Log_Stdout,
                UA_LOGCATEGORY_APPLICATION,
                "A session with the server is activated");
            /* A new session was created. We need to create the subscription. */
            /* Create a subscription */
            UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
            UA_StatusCode retval = UA_Client_Subscriptions_create_async(
                client,
                request,
                NULL,
                NULL,
                deleteSubscriptionCallback,
                createSubscriptionCallback,
                NULL,
                NULL);
            if (retval != UA_STATUSCODE_GOOD)
                UA_LOG_ERROR(
                    UA_Log_Stdout,
                    UA_LOGCATEGORY_APPLICATION,
                    "UA_Client_Subscriptions_create_async ",
                    UA_StatusCode_name(retval));
        } break;
        case UA_SESSIONSTATE_CLOSED:
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION, "Session disconnected");
            break;
        default: break;
    }
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
    }
    UA_String_clear(&str);
}
static int mirua_subscription_create(UA_Client* client, UA_NodeId nodeId) {
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
    // UA_MonitoredItemCreateRequest monRequest2 = UA_MonitoredItemCreateRequest_default(second);
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

int main(int argc, char* argv[]) {
    // DbContext ctx;
    // db_context_init(&ctx);
    // db_connect(&ctx);
    //
    // if (db_exists_in_database(ctx.db, "test")) {
    //     printf("variable exists in database");
    // }
    //
    // memory_arena* vis = arena_create(KB(4));
    // arr_db_schema_variables* ptr = db_query_available_variables(ctx.db, vis);
    // // HexDump(vis->data, KB(4));
    //
    // for (size_t i = 0; i < ptr->count; i++) {
    //     Db_schema_variable var = ptr->items[i];
    //     printf("Hello %s %zu %zu\n", var.name, var.id, var.unit_id);
    // }
    //
    // arena_reset(vis, true);
    //
    // memory_arena* measArena = arena_create(KB(128));
    // Measurement* meas = measurement_create_in_arena(measArena, "Var1", KB(2));
    // db_read_variable_history(ctx.db, meas);
    //
    // // HexDump(measArena->data, KB(5));
    // for (size_t i = 0; i < meas->data.timestamp.count; i++) {
    //     printf(
    //         "Meas: %s ts:%zu v:%f\n",
    //         meas->name,
    //         meas->data.timestamp.items[i],
    //         meas->data.value.items[i]);
    // }
    //
    // db_write_begin(ctx.db);
    // for (size_t i = 0; i < 100; i++) {
    //     int64_t timestamp = 2000 + i;
    //     db_write(ctx.db, timestamp, "Var3", i);
    // }
    //
    // db_write_end();

    //
    //
    //

    // memory_arena* testArena = arena_create(KB(2));
    // arr_i64* test = arr_i64_create_in_arena(testArena, 128);
    //
    // for (size_t i = 0; i < test->capacity; i++) {
    //     int64_t value = 1 + i;
    //     test->items[test->count++] = value;
    //     printf("adding %zu to %zu\n", value, test->count - 1);
    // }
    // HexDump(testArena->data, KB(1));

    // Create a new raw open62541 client for testing
    // UA_Client* testClient = UA_Client_new();
    // UA_ClientConfig_setDefault(UA_Client_getConfig(testClient));
    //
    // UA_StatusCode retval = UA_Client_connect(testClient, "opc.tcp://127.0.0.1:4840");
    // if (retval != UA_STATUSCODE_GOOD) {
    //     log_error("Test client failed to connect: %s", UA_StatusCode_name(retval));
    //     UA_Client_delete(testClient);
    //     return 1;
    // }
    //
    // UA_NodeId nodeId = UA_NODEID_STRING(1, "the.answer");
    // // UA_NodeId secondNode = UA_NODEID_STRING(5, "::Basetime:ticks");
    // mirua_subscription_create(testClient, nodeId);
    //
    // static bool g_running = true;
    // while (g_running) {
    //     UA_Client_run_iterate(testClient, 100);
    // }
    //
    // UA_Client_disconnect(testClient);
    // UA_Client_delete(testClient);

const char* file =
    "[[inputs.opcua.group.nodes]]\n"
    "name = \"Boiler_Temperature\"\n"
    "namespace = \"2\"\n"
    "identifier_type = \"i\"\n"
    "identifier = \"1001\"\n"

    "[[inputs.opcua.group.nodes]]\n"
    "name = \"Boiler_Pressure\"\n"
    "namespace = \"2\"\n"
    "identifier_type = \"i\"\n"
    "identifier = \"1002\"\n"

    "[[inputs.opcua.group.nodes]]\n"
    "name = \"Pump01_Status\"\n"
    "namespace = \"2\"\n"
    "identifier_type = \"s\"\n"
    "identifier = \"Pump01.Status\"\n"

    "[[inputs.opcua.group.nodes]]\n"
    "name = \"Pump01_SpeedRPM\"\n"
    "namespace = \"2\"\n"
    "identifier_type = \"s\"\n"
    "identifier = \"Pump01.SpeedRPM\"\n"

    "[[inputs.opcua.group.nodes]]\n"
    "name = \"Machine_GUID\"\n"
    "namespace = \"3\"\n"
    "identifier_type = \"g\"\n"
    "identifier = \"550e8400-e29b-41d4-a716-446655440000\"\n"

    "[[inputs.opcua.group.nodes]]\n"
    "name = \"Firmware_Blob\"\n"
    "namespace = \"3\"\n"
    "identifier_type = \"b\"\n"
    "identifier = \"RklybXdhcmVCbG9iMDE=\"\n";
    Scanner scanner;
    memory_arena* LexArena = arena_create(KB(64));
    lx_init(&scanner, file, LexArena);
    arr_Tokens* tokens = lx_tokenize(&scanner);
    printf("%s", lx_tokensToStringArena(tokens, LexArena));
    Parser parser;
    parser_init(&parser, tokens, LexArena);
    da_UA_NodeId* nodes = parser_parse(&parser);
    for (size_t i = 0; i < nodes->count; i++) {
        UA_String str = {0};
        UA_NodeId_print(&nodes->items[i], &str);
        printf("%.*s\n", (int)str.length, str.data);
        UA_String_clear(&str);
    }
    return 0;
}
