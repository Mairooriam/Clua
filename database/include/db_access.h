#pragma once
#include <stdint.h>

#include "../core/allocator.h"

#define DB_ARENA_SIZE KB(4)
typedef struct sqlite3 sqlite3;
typedef long long int sqlite_int64;
typedef sqlite_int64 sqlite3_int64;
typedef struct sqlite3_stmt sqlite3_stmt;

/// Ideas to make C style
///
///
//
typedef struct arr_f32 {
    float* items;
    size_t count;
    size_t capacity;
} arr_f32;
arr_f32* arr_f32_create_in_arena(memory_arena* arena, size_t count);

typedef struct arr_i64 {
    int64_t* items;
    size_t count;
    size_t capacity;
} arr_i64;
arr_i64* arr_i64_create_in_arena(memory_arena* arena, size_t count);

typedef struct arr_DataPoints {
    arr_i64 timestamp;
    arr_f32 value;
} arr_DataPoints;
arr_DataPoints* arr_datapoints_create_in_arena(memory_arena* arena, size_t count);

typedef struct Measurement {
    char* name;
    const char* othermetadata;
    memory_arena* arena;
    arr_DataPoints data;
} Measurement;
Measurement* measurement_create_in_arena(
    memory_arena* arena, char* strData, size_t strLenght, size_t count);

typedef struct arr_Measurements {
    Measurement* items;
    size_t count;
    size_t capacity;
} arr_Measurements;

typedef struct DbContext {
    sqlite3* handle;
    sqlite3_stmt* insert_data_stmt;
} DbContext;
void db_context_init(DbContext* ctx);

typedef struct MonitoredItem {
    uint32_t nodeIdx;
    uint32_t subId;
    uint32_t monId;
} MonitoredItem;

typedef struct arr_MonitoredItem {
    MonitoredItem* items;
    size_t count;
    size_t capacity;
} arr_MonitoredItem;

typedef struct Db_schema_variable {
    char* name;
    uint64_t id;
    uint64_t unit_id;
} Db_schema_variable;

typedef struct arr_db_schema_variable {
    Db_schema_variable* items;
    size_t count;
    size_t capacity;
} arr_db_schema_variables;

int db_connect(DbContext* ctx, const char* dbName, char* schema);
int valid_sqlite_result(int rc, sqlite3* db, const char* context);
int db_read_variable_history(sqlite3* db, Measurement* meas);
int db_write_begin(DbContext* db);
void db_write_end(DbContext* ctx);
void db_write(DbContext* ctx, sqlite3_int64 timestamp_ms, const char* name, double value);
arr_db_schema_variables* db_query_available_variables(sqlite3* db, memory_arena* arena);

char* db_read_sql_schema(const char* schemaFilename, memory_arena* arena);
bool db_exists_in_database(sqlite3* db, const char* variable_name);
