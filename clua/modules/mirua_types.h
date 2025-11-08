#pragma once

typedef enum {
    MIRUA_STATE_NORMAL,
    MIRUA_STATE_CONFIG,
    MIRUA_STATE_COUNT,
    MIRUA_STATE_UNINITIALIZED,
} MiruaState;

static inline const char* mirua_state_to_string(MiruaState state) {
    switch (state) {
        case MIRUA_STATE_NORMAL:
            return "NORMAL";
        case MIRUA_STATE_CONFIG:
            return "CONFIG";
        case MIRUA_STATE_COUNT:
            return "COUNT";
        case MIRUA_STATE_UNINITIALIZED:
            return "UNINITIALIZED";
        default:
            return "UNKNOWN";
    }
}