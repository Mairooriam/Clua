#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mirua_types.h"
typedef struct MiruaContext MiruaContext;
typedef struct MiruaConfig MiruaConfig;
MiruaContext* mirua_module_create(void);
void mirua_module_free(MiruaContext* ctx);

// API
int mirua_connect(MiruaContext* ctx, const char* endpoint);
int mirua_disconnect(MiruaContext* ctx);
void mirua_exploreNodes(MiruaContext* ctx, const char* node, int idx);
int mirua_navigate_down(MiruaContext* ctx, size_t index);
int mirua_navigate_up(MiruaContext* ctx);
int mirua_navigate_to(MiruaContext* ctx, const char* nodeid_str);
void mirua_save(MiruaContext* ctx, int idx, int start, int end);
MiruaState mirua_state_get(MiruaContext* ctx);
void mirua_state_change(MiruaContext* ctx, MiruaState newState);
void mirua_client_iterate(MiruaContext* ctx, uint32_t timeout);

void mirua_config_set(MiruaConfig* config, const char* key, const char* value);
void mirua_config_print_ctx(MiruaContext* ctx);
void mirua_config_print_by_idx(MiruaContext* ctx, size_t idx);
void mirua_config_set_by_idx_ctx(MiruaContext* ctx, size_t idx, const char* value);
const char* mirua_config_get_current_endpoint(MiruaContext* ctx);
