#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

typedef struct sqlite3 sqlite3;
typedef long long int sqlite_int64;
typedef sqlite_int64 sqlite3_int64;

/// Ideas to make C style
///
///
//
// typedef struct arr_f32 {
//     float* items;
//     size_t count;
//     size_t capacity;
// } arr_f32;
//
// typedef struct arr_u64 {
//     uint64_t* items;
//     size_t count;
//     size_t capacity;
// } arr_u64;
//
// typedef struct arr_DataPoints {
//     arr_u64 timestamp;
//     arr_f32 value;
// } arr_DataPoints;
//
// typedef struct Measurement {
//     const char* name;
//     const char* othermetadata;
//     arr_DataPoints data;
// } Measurement;
//
// typedef struct arr_Measurements {
//     Measurement* items;
//     size_t count;
//     size_t capacity;
// } arr_Measurements;
//
typedef struct DbContext {
    std::string dbName = "";
    sqlite3* db = nullptr;
    std::string dbSchemaFilename = "";
} DbContext;

struct db_schema_variable {
    uint64_t id = 0;
    std::string name = "";
    uint64_t unit_id = 0;
};

struct Measurements {
    std::vector<int64_t> timestamp;
    std::vector<double> value;
};

// TODO: wrap this in context to have selected stuff. etc. more info
using MeasurementRecord = std::unordered_map<std::string, Measurements>;

int db_connect(DbContext* ctx);
int valid_sqlite_result(int rc, sqlite3* db, const char* context);
int read_variable_history(sqlite3* db, const char* variable_name, MeasurementRecord* record);
int init_test_data_writer(sqlite3* db);
void insert_test_value(sqlite3* db, sqlite3_int64 timestamp_ms, const char* name, double value);
void shutdown_test_data_writer();
int db_query_variable_info(sqlite3* db, std::vector<db_schema_variable>& variables);

std::string db_read_sql_file_filtered(const char* filename);
bool db_exists_in_database(sqlite3* db, const char* variable_name);
