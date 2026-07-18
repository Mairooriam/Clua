#include <math.h>
#include <open62541/server.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "open62541/common.h"
#include "open62541/types.h"

static volatile sig_atomic_t running = 1;
static void stopHandler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    printf("Hello world\n");
    UA_Server* server = UA_Server_new();
    if (!server) return EXIT_FAILURE;

    /* Add a sine float variable */
    UA_VariableAttributes attrSin = UA_VariableAttributes_default;
    float sinInitial = 0.0f;
    UA_Variant_setScalarCopy(&attrSin.value, &sinInitial, &UA_TYPES[UA_TYPES_FLOAT]);
    attrSin.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
    attrSin.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", "Sine");
    UA_NodeId sinNodeId = UA_NODEID_STRING_ALLOC(1, "sin.value");
    UA_QualifiedName sinName = UA_QUALIFIEDNAME_ALLOC(1, "Sine");
    UA_Server_addVariableNode(
        server,
        sinNodeId,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        sinName,
        UA_NODEID_NULL,
        attrSin,
        NULL,
        NULL);
    UA_VariableAttributes_clear(&attrSin);

    /* Add a counter int32 variable */
    UA_VariableAttributes attrCnt = UA_VariableAttributes_default;
    UA_Int32 cntInitial = 0;
    UA_Variant_setScalarCopy(&attrCnt.value, &cntInitial, &UA_TYPES[UA_TYPES_INT32]);
    attrCnt.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    attrCnt.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", "Counter");
    UA_NodeId cntNodeId = UA_NODEID_STRING_ALLOC(1, "counter.value");
    UA_QualifiedName cntName = UA_QUALIFIEDNAME_ALLOC(1, "Counter");
    UA_Server_addVariableNode(
        server,
        cntNodeId,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        cntName,
        UA_NODEID_NULL,
        attrCnt,
        NULL,
        NULL);
    UA_VariableAttributes_clear(&attrCnt);

    /* Add a time string variable */
    UA_VariableAttributes attrTime = UA_VariableAttributes_default;
    UA_String timeInit = UA_STRING_NULL;
    UA_Variant_setScalarCopy(&attrTime.value, &timeInit, &UA_TYPES[UA_TYPES_STRING]);
    attrTime.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    attrTime.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", "Time");
    UA_NodeId timeNodeId = UA_NODEID_STRING_ALLOC(1, "time.value");
    UA_QualifiedName timeName = UA_QUALIFIEDNAME_ALLOC(1, "Time");
    UA_Server_addVariableNode(
        server,
        timeNodeId,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        timeName,
        UA_NODEID_NULL,
        attrTime,
        NULL,
        NULL);
    UA_VariableAttributes_clear(&attrTime);

    /* Start server internal services */
    UA_StatusCode rc = UA_Server_run_startup(server);
    if (rc != UA_STATUSCODE_GOOD) {
        UA_Server_delete(server);
        return EXIT_FAILURE;
    }

    /* Update loop */
    double t = 0.0;
    const double dt = 0.1; /* seconds per step */
    int32_t counter = 0;

    struct timespec sleepReq = {0, 100 * 1000000}; /* 100ms */

    while (running) {
        /* sine */
        t += dt;
        float s = sinf((float)t);
        UA_Variant v;
        UA_Variant_setScalarCopy(&v, &s, &UA_TYPES[UA_TYPES_FLOAT]);
        UA_Server_writeValue(server, sinNodeId, v);
        UA_Variant_clear(&v);

        /* counter */
        counter++;
        UA_Variant_setScalarCopy(&v, &counter, &UA_TYPES[UA_TYPES_INT32]);
        UA_Server_writeValue(server, cntNodeId, v);
        UA_Variant_clear(&v);

        /* time string */
        time_t now = time(NULL);
        char buf[64] = "";
        struct tm tmv;
        localtime_r(&now, &tmv);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
        UA_String uas = UA_String_fromChars(buf);
        UA_Variant_setScalarCopy(&v, &uas, &UA_TYPES[UA_TYPES_STRING]);
        UA_Server_writeValue(server, timeNodeId, v);
        UA_Variant_clear(&v);
        UA_String_clear(&uas);

        /* print to stdout */
        // printf("Sine=%.4f  Counter=%d  Time=%s\n", s, counter, buf);

        /* run server iteration and sleep a bit */
        UA_Server_run_iterate(server, false);
        nanosleep(&sleepReq, NULL);
    }

    /* Shutdown */
    UA_Server_run_shutdown(server);
    UA_Server_delete(server);

    /* Clear allocated NodeId / QualifiedName */
    UA_NodeId_clear(&sinNodeId);
    UA_NodeId_clear(&cntNodeId);
    UA_NodeId_clear(&timeNodeId);
    UA_QualifiedName_clear(&sinName);
    UA_QualifiedName_clear(&cntName);
    UA_QualifiedName_clear(&timeName);

    return EXIT_SUCCESS;
}
