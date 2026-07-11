

#include <stdint.h>
#include <stdio.h>

#include "core/allocator.h"
#include "include/db_access.h"
#include "open62541/client.h"
#include "open62541/client_config_default.h"
#include "open62541/client_highlevel.h"
#include "open62541/client_subscriptions.h"

int main(int argc, char* argv[]) {
    UA_Client* client;
    // DbContext ctx;
    // db_context_init(&ctx);
    // db_connect(&ctx);
    //
    // if (db_exists_in_database(ctx.db, "test")) {
    //     printf("variable exists in database");
    // }
    //
    // memory_arena* vis = arena_create(KB(4));
    // arr_db_schema_variables* ptr = db_query_available_variables(ctx.db, vis);
    // // HexDump(vis->data, KB(4));
    //
    // for (size_t i = 0; i < ptr->count; i++) {
    //     Db_schema_variable var = ptr->items[i];
    //     printf("Hello %s %zu %zu\n", var.name, var.id, var.unit_id);
    // }
    //
    // arena_reset(vis, true);
    //
    // memory_arena* measArena = arena_create(KB(128));
    // Measurement* meas = measurement_create_in_arena(measArena, "Var1", KB(2));
    // db_read_variable_history(ctx.db, meas);
    //
    // // HexDump(measArena->data, KB(5));
    // for (size_t i = 0; i < meas->data.timestamp.count; i++) {
    //     printf(
    //         "Meas: %s ts:%zu v:%f\n",
    //         meas->name,
    //         meas->data.timestamp.items[i],
    //         meas->data.value.items[i]);
    // }
    //
    // db_write_begin(ctx.db);
    // for (size_t i = 0; i < 100; i++) {
    //     int64_t timestamp = 2000 + i;
    //     db_write(ctx.db, timestamp, "Var3", i);
    // }
    //
    // db_write_end();
    //
    //
    //
    //

    // memory_arena* testArena = arena_create(KB(2));
    // arr_i64* test = arr_i64_create_in_arena(testArena, 128);
    //
    // for (size_t i = 0; i < test->capacity; i++) {
    //     int64_t value = 1 + i;
    //     test->items[test->count++] = value;
    //     printf("adding %zu to %zu\n", value, test->count - 1);
    // }
    // HexDump(testArena->data, KB(1));

    printf("Hello world");
    return 0;
}
