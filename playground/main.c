#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log.h>
#include <open62541/types.h>
#include <open62541/util.h>
#include <stdio.h>
#include <string.h>

#include "../clua/platform/platform.h"
#include "../clua/src/log.h"

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <test_name>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "1") == 0) {
        printf("MSCV thingy: %d\n", _MSC_FULL_VER);

        UA_Client* client = UA_Client_new();

        const char* endpoint = "opc.tcp://127.0.0.1:4840";

        UA_StatusCode result = UA_Client_connect(client, endpoint);
        if (result != UA_STATUSCODE_GOOD) {
            log_error(
                "Failed to connect to %s with UA_statusCode: %s",
                endpoint,
                UA_StatusCode_name(result));
        } else {
            log_info("Connection to %s was succesfull", endpoint);
        }

        UA_NodeId* node = UA_NodeId_new();
        UA_String str = UA_String_fromChars("ns=6;s=::AsGlobalPV:hmi_pids.EX_LC001");
        UA_NodeId_parse(node, str);

        UA_NodeClass class;
        UA_NodeClass_init(&class);
        UA_StatusCode status = UA_Client_readNodeClassAttribute(client, *node, &class);
        if (status != UA_STATUSCODE_GOOD) {
            log_error("UA_Client_readNodeClassAttribute returned != UA_STATUSCODE_GOOD");
            return false;
        }

        log_info("got NodeClass: %d, expected %d", class, 2);

        log_warn("penis");
    } else if (strcmp(argv[1], "2") == 0) {
        const char* str = "0 test";
        int index;
        char value[128];

        // Parse the string into specific variables
        if (sscanf(str, "%d %127[^\n]", &index, value) == 2) {
            printf("Index: %d\n", index);
            printf("Value: %s\n", value);
        } else {
            printf("Invalid input format.\n");
        }

        printf("Running Test 2...\n");
    } else if (strcmp(argv[1], "3") == 0) {
        const char* args = "10 11";
        int idx1 = INT_MAX, idx2 = INT_MAX;
        if (sscanf(args, "%d %d", &idx1, &idx2) == 1) {
            printf("hello parsed 1 number 1:%d 2:%d\n", idx1, idx2);
        } else if (sscanf(args, "%d %d", &idx1, &idx2) == 2) {
            printf("parsed 2 1:%d 2:%d\n", idx1, idx2);
        }

        printf("Running Test 3...\n");
    } else if (strcmp(argv[1], "4") == 0) {
        // TODO: proper tests when!?
        MirFile file = {0};
        MirFileResult result = MirFileOpen("test.txt", &file, MIR_FILE_ACCESS_APPEND);
        if (result != MIR_FILE_OK) {
            printf("Error opening file\n");
            return 1;
        }

        const char* data = "Appending this line.\n";
        size_t bytesWritten;
        result = MirFileWrite(&file, data, strlen(data), &bytesWritten);
        if (result != MIR_FILE_OK) {
            printf("Error writing to file\n");
        } else {
            size_t fileSize = 0;
            if (MirFileSize(&file, &fileSize) != MIR_FILE_OK) {
                printf("Error in reading file Size");
            }
            printf("wrote to file %s, bytes:%zu, size:%zu", "test.txt", bytesWritten, fileSize);
        }

        MirFileClose(&file);

    } else {
        printf("Unknown test: %s\n", argv[1]);
        printf("Available tests: test1, test2, test3\n");
    }

    return 0;
}
