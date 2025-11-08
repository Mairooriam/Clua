#include "mirua_serialization.h"

#include <stdio.h>
#include <string.h>

#include "mirua_types_internal.h"
#include "open62541/types.h"

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
    offset += snprintf(buf + offset, bufsize - offset, "[[inputs.opcua.nodes]]\n");
    offset += snprintf(
        buf + offset,
        bufsize - offset,
        "name = \"%.*s\"\n",
        (int)node->name.name.length,
        node->name.name.data);
    offset += snprintf(
        buf + offset, bufsize - offset, "namespace = \"%u\"\n", node->nodeid.namespaceIndex);

    if (node->nodeid.identifierType == UA_NODEIDTYPE_STRING) {
        offset += snprintf(buf + offset, bufsize - offset, "identifier_type = \"%s\"\n", "s");
        offset += snprintf(
            buf + offset,
            bufsize - offset,
            "identifier = \"%.*s\"\n",
            (int)node->nodeid.identifier.string.length,
            node->nodeid.identifier.string.data);
    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_NUMERIC) {
        offset += snprintf(buf + offset, bufsize - offset, "identifier_type = \"%s\"\n", "i");
        offset += snprintf(
            buf + offset,
            bufsize - offset,
            "identifier = \"%u\"\n",
            node->nodeid.identifier.numeric);

    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_BYTESTRING) {
        assert(0 && "notimplmeented");
    } else if (node->nodeid.identifierType == UA_NODEIDTYPE_GUID) {
        assert(0 && "not implemented");
    }

    return offset;
}
static size_t telegraf_serialize_nodes(char* buf, size_t bufsize, const mirua_t_NodeList* nodes) {
    size_t offset = 0;
    offset += snprintf(buf + offset, bufsize - offset, "#GENERATED NODES\n");
    for (size_t i = 0; i < nodes->size; i++) {
        offset += telegraf_serialize_node(buf + offset, bufsize - offset, &nodes->nodeIds[i]);
    }
    return offset;
}
// static int csv_deserialize_nodes(const char* buf, mirua_t_nodelist* out_nodes) {
//     (void)buf;
//     (void)out_nodes;
//     return 0;
// }

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
        .save_to_file = NULL,
        .format_name = "telegraf"};

    switch (format) {
        // case SERIALIZER_CSV: return &csv_serializer;
        case SERIALIZER_TELEGRF: return &telegraf_serializer;
        default: return NULL;
    }
}
