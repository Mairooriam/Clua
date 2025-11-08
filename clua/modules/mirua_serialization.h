#pragma once
#include <mirua_types_internal.h>
#include <stdint.h>

// Serializer interface
typedef struct {
    size_t (*serialize_node)(char* buf, size_t bufsize, const MiruaNodeId* node);
    size_t (*serialize_nodes)(char* buf, size_t bufsize, const mirua_t_NodeList* nodes);
    size_t (*deserialize_nodes)(char* buf, size_t bufsize, mirua_t_NodeList* out_nodes);
    size_t (*save_to_file)(const mirua_t_NodeList* nodes, const char* filename);
    size_t (*load_from_file)(mirua_t_NodeList* out_nodes, const char* filename);
    const char* format_name;
} MiruaNodeSerializer;

typedef enum {
    SERIALIZER_JSON,
    SERIALIZER_CSV,
    SERIALIZER_TELEGRF,
    SERIALIZER_COUNT,
} SerializerFormat;

MiruaNodeSerializer* mirua_serializer_get(SerializerFormat format);
