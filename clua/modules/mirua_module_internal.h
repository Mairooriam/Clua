#pragma once

#include "log.h"
#include "mirua_module.h"
#include "mirua_types_internal.h"
#include "open62541/client.h"
#include "open62541/types.h"
// NODE LIST
// TODO: ls filter
//  -> shows filters if active etc. numbers
//  -> select set filter 1-10 on
//  -> etc. or set filter 1 4 5 6 9
//  off filter 1 4 5 6 9
//  TODO re think this filtering this is awful
#define DEFAULT_DATATYPE_FILTER_MASK \
    ((1U << UA_DATATYPEKIND_INT32) | (1U << UA_DATATYPEKIND_STRING))
typedef enum {
    MIRUA_FILTER_INVALID,
    MIRUA_FILTER_ALL,
    MIRUA_FILTER_UA_TYPES,
    MIRUA_FILTER_COUNT,
} MiruaFilterType;
typedef bool (*MiruaNodeFilter)(
    UA_NodeId childId, UA_NodeId referenceTypeId, UA_Client* client, void* userData);
bool mirua_callback_filter_ALL(
    UA_NodeId childId, UA_NodeId referenceTypeId, UA_Client* client, void* data);
bool mirua_callback_filter(
    UA_NodeId childId, UA_NodeId referenceTypeId, UA_Client* client, void* data);
bool mirua_filter_is_numeric_or_bool(UA_Client* client, const UA_NodeId node);
MiruaNodeFilter mirua_filter_get_func(MiruaFilterType type);
const char* mirua_filter_get_name(MiruaFilterType type);
MiruaNodeFilter mirua_filter_get_current_func(MiruaContext* ctx);
MiruaFilterType mirua_filter_parse_type(const char* str);
typedef struct MiruaConfig {
    char* endpoint;
    UA_NodeId defaultRoot;
    uint32_t selectedDataTypeKinds;  // maps to UA_DATATYPEKIND s. mask
    MiruaFilterType filterType;
    int printLevel;
    char* output_path;
} MiruaConfig;

// MAIN CONTEXT
typedef struct MiruaContext {
    UA_Client* client;
    UA_ClientConfig* ua_config;
    bool connected;
    MiruaNodeId currentNode;
    mirua_t_NodeList currentChildren;
    NodeIdHistory history;
    MiruaConfig config;
    MiruaState state;
} MiruaContext;

typedef size_t (*mirua_fn_toStringNodeId)(
    char* buf, size_t bufsize, UA_Client* client, const UA_NodeId* node);
typedef struct {
    mirua_t_NodeList* nodes;
    UA_Client* client;
    MiruaNodeFilter filter;
    void* userData;  //??? not sure if i will need this
} MiruaCallbackHandle;
UA_StatusCode mirua_cb_printNode(
    UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle);
UA_StatusCode mirua_cb_printNodeEx(
    UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle);
UA_StatusCode mirua_cb_collectNodes_to_nodelist(
    UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle);

UA_StatusCode mirua_cb_nodeIter(
    UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle);
size_t mirua_to_string_nodeId(char* buf, size_t bufsize, UA_Client* client, const UA_NodeId* node);
size_t mirua_to_string_nodeIdEx(
    char* buf,
    size_t bufsize,
    UA_Client* client,
    const UA_NodeId*
        node);  // TODO: make dynamic string getter and think of function to fetch data.
size_t mirua_to_string_nodeId_datatype(
    char* buf, size_t bufsize, UA_Client* client, const UA_NodeId* node);

// TODO: add children to history?
void mirua_history_addToHistory(NodeIdHistory* hist, const MiruaNodeId* nodeId);
int mirua_history_goBack(NodeIdHistory* hist);
int mirua_history_to_string(
    char* buf,
    size_t bufsize,
    UA_Client* client,
    NodeIdHistory* hist,
    mirua_fn_toStringNodeId func);

void mirua_printNodeDataType(UA_Client* client, const UA_NodeId* nodeId);
void mirua_print_node(MiruaContext* ctx, const UA_NodeId* node, int indent);
void _mirua_print_node(
    char* buf,
    size_t bufsize,
    UA_Client* client,
    int printLevel,
    const UA_NodeId* node,
    int indent);

void mirua_print_current_children(MiruaContext* ctx);
void mirua_print_current_node(MiruaContext* ctx);
void mirua_print_node_path(
    UA_Client* client, const UA_NodeId* nodeId);  // TODO: delete if not needed just testing
void mirua_print_node_value_json(
    UA_Client* client, const UA_NodeId* nodeId);  // TODO: delete if not neede just for tesintg
// void _mirua_exploreNodes_by_nodeId(
//     UA_Client* client, mirua_t_NodeList* nodes, const UA_NodeId node);
bool mirua_node_exists(UA_Client* client, const UA_NodeId* node);
void mirua_explore_children(MiruaContext* ctx, mirua_t_NodeList* nodes, const UA_NodeId* node);

// CONFIG

typedef enum {
    MIRUA_CONFIG_ENDPOINT,
    MIRUA_CONFIG_TYPE_STRING,
    MIRUA_CONFIG_TYPE_NODEID,
    MIRUA_CONFIG_TYPE_FILTER,
    MIRUA_CONFIG_TYPE_FILTER_FUNC,
    MIRUA_CONFIG_TYPE_PRINT_LEVEL,
    MIRUA_CONFIG_TYPE_FILE_OUTPUT_PATH,
    MIRUA_CONFIG_COUNT,
} MiruaConfigType;

typedef struct {
    const char* name;
    MiruaConfigType type;
    size_t offset;
} MiruaConfigMapping;
void mirua_config_set_ctx(MiruaContext* ctx, const char* key, const char* value);
void mirua_config_set_by_idx(MiruaConfig* config, size_t idx, const char* value);
void mirua_config_print_field(
    const MiruaConfig* config, const MiruaConfigMapping* map, bool detailed);
void mirua_config_print_by_idx(MiruaContext* ctx, size_t idx);
void mirua_config_print(const MiruaConfig* config);
bool mirua_config_load_from_file(MiruaConfig* config, const char* filepath);
void mirua_config_print_enabled_data_types(uint32_t mask);
bool mirua_config_validate_endpoint(const char* endpoint);
uint32_t mirua_parse_filters(const char* filters_str);
extern const MiruaConfigMapping configMapping[];
extern const size_t mirua_config_mapping_count;

void mirua_explore_value(MiruaContext* ctx, const UA_NodeId* nodeId);

const char* mirua_node_type_to_string(MiruaNodeType type);

size_t mirua_tree_node_to_string(
    char* buf, size_t bufsize, const MiruaTreeNode* node, size_t indent);
size_t mirua_tree_to_string(char* buf, size_t bufsize, const MiruaTree* node);

MiruaTree* mirua_tree_create(MiruaTreeNode* root);
void mirua_tree_destroy(MiruaTree* tree);
void mirua_tree_add_child(MiruaTree* tree, MiruaTreeNode* parent, MiruaTreeNode* child);
void mirua_tree_node_destroy(MiruaTreeNode* node);
size_t mirua_tree_count_nodes(const MiruaTree* tree);
MiruaTreeNode* _mirua_tree_node_create(
    const MiruaNodeId* nodeId, const MiruaValue* value, const UA_NodeId* type);
MiruaTreeNode* mirua_tree_node_create(UA_Client* client, const UA_NodeId node);
void mirua_tree_collect_values(mirua_t_NodeList* nodes, MiruaTree* tree);
// structure
void _mirua_build_structure_tree(
    UA_Client* client, MiruaTree* tree, MiruaTreeNode* parent, const UA_NodeId node);
MiruaTree* mirua_explore_structure(UA_Client* client, const UA_NodeId node);
bool mirua_nodeId_is_structure(UA_Client* client, const UA_NodeId node);
bool mirua_nodeId_nodeClass_is(UA_Client* client, const UA_NodeId node, UA_NodeClass class_in);

// MiruaNodeId functions
void mirua_nodeId_init(MiruaNodeId* nodeId);
void mirua_nodeId_clear(MiruaNodeId* nodeId);
MiruaNodeId* _mirua_nodeId_create(const UA_NodeId nodeId, const UA_QualifiedName* name);
MiruaNodeId* mirua_nodeId_create(UA_Client* client, const UA_NodeId nodeId);
void mirua_nodeId_destroy(MiruaNodeId* nodeId);
size_t mirua_nodeId_to_string(char* buf, size_t bufisze, const MiruaNodeId* nodeid, size_t indent);
void mirua_nodeId_copy(const MiruaNodeId* src, MiruaNodeId* dst);

// MiruaValue functions
void mirua_value_init(MiruaValue* value);
void mirua_value_clear(MiruaValue* value);
MiruaValue* _mirua_value_create(const UA_Variant* value, const MiruaNodeId* type);
MiruaValue* mirua_value_create(UA_Client* client, const UA_NodeId nodeId);
void mirua_value_destroy(MiruaValue* value);
size_t mirua_value_to_string(char* buf, size_t bufsize, const MiruaValue* value, size_t indent);

static inline void mirua_init_nodeList(mirua_t_NodeList* nodes, size_t capacity) {
    nodes->nodeIds = (MiruaNodeId*)malloc(sizeof(MiruaNodeId) * capacity);
    nodes->capacity = capacity;
    nodes->size = 0;
    log_alloc("[NODELIST] - created");
}
static inline void mirua_clear_nodeList(mirua_t_NodeList* nodes) {
    for (size_t i = 0; i < nodes->size; ++i) {
        mirua_nodeId_clear(&nodes->nodeIds[i]);
    }
    nodes->size = 0;
}

static inline void mirua_free_nodeList(mirua_t_NodeList* nodes) {
    mirua_clear_nodeList(nodes);
    if (nodes->nodeIds) {
        free(nodes->nodeIds);
        nodes->nodeIds = NULL;
    }
    nodes->capacity = 0;
    log_de_alloc("[NODELIST] - freed");
}
