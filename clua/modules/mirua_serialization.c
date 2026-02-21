#include "mirua_serialization.h"

#include <stdio.h>
#include <string.h>

#include "mirua_types_internal.h"
#include "open62541/types.h"
#include "utils.h"
#define SERIALIZER_TO_FILE_INITIAL_MEMORY 1024 * 10

// CSV
// static int _csv_serialize_node(const UA_NodeId* node, char* buf, size_t bufsize, size_t* offset)
// {
//     UA_String str;
//     UA_String_init(&str);
//     UA_NodeId_print(node, &str);
//     int written = snprintf(buf + *offset, bufsize - *offset, "%.*s\n", (int)str.length,
//     str.data); UA_String_clear(&str);
//
//     if (written < 0 || (size_t)written >= bufsize - *offset) {
//         return -1;
//     }
//     *offset += (size_t)written;
//     return 0;
// }
//
// static int _csv_header(char* buf, size_t bufsize, size_t* offset) {
//     const char* header = "namespace;identifier\n";
//     int written = snprintf(buf + *offset, bufsize - *offset, "%s", header);
//     if (written < 0 || (size_t)written >= bufsize - *offset) {
//         return -1;
//     }
//     *offset += (size_t)written;
//     return 0;
// }
//
// static int csv_serialize_node(const UA_NodeId* node, char* buf, size_t bufsize) {
//     size_t offset = 0;
//
//     if (_csv_header(buf, bufsize, &offset) != 0) {
//         return -1;
//     }
//
//     if (_csv_serialize_node(node, buf, bufsize, &offset) != 0) {
//         return -1;
//     }
//     return 0;
// }
// static int csv_serialize_nodes(
//     const mirua_t_NodeList* nodes, char* buf, size_t bufsize, int start, int end) {
//     size_t offset = 0;
//     if (_csv_header(buf, bufsize, &offset) != 0) return -1;
//     if (start < 0 || end < 0 || start > end || end >= (int)nodes->size) {
//         start = 0;
//         end = (int)nodes->size - 1;
//     }
//     for (int i = start; i <= end; ++i) {
//         if (_csv_serialize_node(&nodes->nodeIds[i].nodeid, buf, bufsize, &offset) != 0) return
//         -1;
//     }
//     return 0;
// }
// static int csv_deserialize_nodes(const char* buf, mirua_t_NodeList* out_nodes) {
//     (void)buf;
//     (void)out_nodes;
//     return 0;
// }

// TELEGRAF

static size_t telegraf_serialize_node(char* buf, size_t bufsize, const MiruaNodeId* node) {
    size_t offset = 0;

    if (safe_snprintf(buf, &offset, bufsize, "[[inputs.opcua.nodes]]\n") != 0) return SIZE_MAX;
    if (safe_snprintf(buf, &offset, bufsize, "name = \"%.*s\"\n",
                      (int)node->name.name.length, node->name.name.data) != 0) return SIZE_MAX;
    if (safe_snprintf(buf, &offset, bufsize, "namespace = \"%u\"\n", node->nodeid.namespaceIndex) != 0) return SIZE_MAX;

    if (node->nodeid.identifierType == UA_NODEIDTYPE_STRING) {
        if (safe_snprintf(buf, &offset, bufsize, "identifier_type = \"%s\"\n", "s") != 0) return SIZE_MAX;
        if (safe_snprintf(buf, &offset, bufsize, "identifier = \"%.*s\"\n",
                          (int)node->nodeid.identifier.string.length, node->nodeid.identifier.string.data) != 0) return SIZE_MAX;
    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_NUMERIC) {
        if (safe_snprintf(buf, &offset, bufsize, "identifier_type = \"%s\"\n", "i") != 0) return SIZE_MAX;
        if (safe_snprintf(buf, &offset, bufsize, "identifier = \"%u\"\n", node->nodeid.identifier.numeric) != 0) return SIZE_MAX;
    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_BYTESTRING) {
        assert(0 && "not implemented");
        return SIZE_MAX;
    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_GUID) {
        assert(0 && "not implemented");
        return SIZE_MAX;
    }

    return offset;
}

static size_t telegraf_serialize_nodes(char* buf, size_t bufsize, const mirua_t_NodeList* nodes) {
    size_t offset = 0;

    if (safe_snprintf(buf, &offset, bufsize, "#GENERATED NODES\n") != 0) return SIZE_MAX;

    for (size_t i = 0; i < nodes->size; i++) {
        size_t node_written = telegraf_serialize_node(buf + offset, bufsize - offset, &nodes->nodeIds[i]);
        if (node_written == SIZE_MAX) return SIZE_MAX;
        offset += node_written;
        if (safe_snprintf(buf, &offset, bufsize, "\n") != 0) return SIZE_MAX;
    }

    return offset;
}
static size_t telegraf_serialize_nodes_to_file(const mirua_t_NodeList* nodes, FILE* file) {
    size_t bufsize = SERIALIZER_TO_FILE_INITIAL_MEMORY;
    char* buf = malloc(bufsize);
    if (!buf) return SIZE_MAX;

    size_t written;
    while ((written = telegraf_serialize_nodes(buf, bufsize, nodes)) == SIZE_MAX) {
        size_t new_bufsize = bufsize * 2;
        char* new_buf = realloc(buf, new_bufsize);
        if (!new_buf) {
            free(buf);
            return SIZE_MAX;
        }
        buf = new_buf;
        bufsize = new_bufsize;
    }

    if (fwrite(buf, 1, written, file) != written) {
        free(buf);
        return SIZE_MAX;
    }

    free(buf);
    return 0;
}
MiruaNodeSerializer* mirua_serializer_get(SerializerFormat format) {
    // static MiruaNodeSerializer csv_serializer = {
    //     .serialize_node = csv_serialize_node,
    //     .serialize_nodes = csv_serialize_nodes,
    //     .deserialize_nodes = csv_deserialize_nodes,
    //     .save_to_file = NULL,
    //     .load_from_file = NULL,
    //     .format_name = "csv"};

    static MiruaNodeSerializer telegraf_serializer = {
        .serialize_node = telegraf_serialize_node,
        .serialize_nodes = telegraf_serialize_nodes,
        .deserialize_nodes = NULL,
        .serialize_nodes_to_file = telegraf_serialize_nodes_to_file,
        .save_to_file = NULL,
        .format_name = "telegraf"};

    switch (format) {
        // case SERIALIZER_CSV: return &csv_serializer;
        case SERIALIZER_TELEGRF: return &telegraf_serializer;
        default: return NULL;
    }
}
