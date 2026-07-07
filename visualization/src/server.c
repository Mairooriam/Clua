#include <open62541/server.h>
#include <stdio.h>

#include "open62541/common.h"
#include "open62541/types.h"
int main(int argc, char* argv[]) {
    printf("Hello world");
    UA_Server* server = UA_Server_new();
    int running = 1;
    if (running) {
        // add a variable node to the adresspace
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        UA_Float myInteger = 42.5f;
        UA_Variant_setScalarCopy(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_FLOAT]);
        attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;

        attr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", "the answer");
        attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", "the answer");
        UA_NodeId myIntegerNodeId = UA_NODEID_STRING_ALLOC(1, "the.answer");
        UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME_ALLOC(1, "the answer");
        UA_NodeId parentNodeId = UA_NS0ID(OBJECTSFOLDER);
        UA_NodeId parentReferenceNodeId = UA_NS0ID(ORGANIZES);
        attr.minimumSamplingInterval = 10;
        attr.valueRank = UA_VALUERANK_SCALAR;
        attr.accessLevel = 255;
        attr.userAccessLevel = 255;

        printf("acceslevel:%d", attr.userAccessLevel);

        UA_Server_addVariableNode(
            server,
            myIntegerNodeId,
            parentNodeId,
            parentReferenceNodeId,
            myIntegerName,
            UA_NODEID_NULL,
            attr,
            NULL,
            NULL);

        /* allocations on the heap need to be freed */
        UA_VariableAttributes_clear(&attr);
        UA_NodeId_clear(&myIntegerNodeId);
        UA_QualifiedName_clear(&myIntegerName);

        UA_StatusCode retval = UA_Server_runUntilInterrupt(server);

        UA_Server_delete(server);
        return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    UA_Server_delete(server);

    return 0;
}
