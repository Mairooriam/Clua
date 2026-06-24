#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "sqlite3.h"

typedef struct DbContext {
    std::string dbName = "";
    sqlite3* db = nullptr;
    std::string dbSchemaFilename = "";
} DbContext;

struct Measurements {
    std::vector<int64_t> timestamp;
    std::vector<double> value;
};
using MeasurementRecord = std::unordered_map<std::string, Measurements>;

int db_connect(DbContext* ctx);
int check_sqlite_result(int rc, sqlite3* db, const char* context);
int read_variable_history(sqlite3* db, const char* variable_name, MeasurementRecord* record);
int init_test_data_writer(sqlite3* db);
void insert_test_value(sqlite3* db, sqlite3_int64 timestamp_ms, int variable_id, double value);
void shutdown_test_data_writer();

std::string db_read_sql_file_filtered(const char* filename);
bool db_exists_in_database(sqlite3* db, const char* variable_name);
