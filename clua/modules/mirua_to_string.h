#pragma once
#include <open62541/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* mirua_str_status_code(uint32_t code);
const char* mirua_str_secure_channel_state(UA_SecureChannelState state);
const char* mirua_str_session_state(UA_SessionState state);
const char* mirua_str_node_class(UA_NodeClass nodeClass);
const char* mirua_to_string_attribute_id(uint32_t attrId);
int mirua_to_string_attr_value(char* buf, int* val);

#ifdef __cplusplus
}
#endif
