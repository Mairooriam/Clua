
#include "db_access.h"

#include "log.h"
#include "sqlite3.h"

static sqlite3_stmt* insert_data_stmt = nullptr;

int db_connect(DbContext* ctx) {
    if (ctx->dbName.empty()) {
        log_error("db_connect called without db name!");
        return 0;
    }

    if (ctx->db) {
        log_warn("db_connect called with existing db. possibly not wanted behavior!");
        return 0;
    }

    if (ctx->dbSchemaFilename.empty()) {
        log_warn("db_connect Called without dbSchemaFilename!");
        return 0;
    }

    std::string sql = db_read_sql_file_filtered(ctx->dbSchemaFilename.c_str());

    int sql_rc = sqlite3_open(ctx->dbName.c_str(), &ctx->db);
    if (!valid_sqlite_result(sql_rc, ctx->db, "db_connect")) {
        if (ctx->db) sqlite3_close(ctx->db);
        return 0;
    }

    sql_rc = sqlite3_exec(ctx->db, sql.c_str(), nullptr, nullptr, nullptr);
    if (!valid_sqlite_result(sql_rc, ctx->db, "db_connect")) {
        if (ctx->db) sqlite3_close(ctx->db);
        return 0;
    }
    return 1;
}

int valid_sqlite_result(int rc, sqlite3* db, const char* context) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        log_error("[sqlite] %s failed: %s\n", context, db ? sqlite3_errmsg(db) : "unknown");
        return 0;
    } else {
        return 1;
    }
}
bool db_exists_in_database(sqlite3* db, const char* variable_name) {
    sqlite3_stmt* exists_stmt = nullptr;
    const char* exists_sql = "SELECT 1 FROM variable WHERE name = ?1 LIMIT 1;";
    int rc = sqlite3_prepare_v2(db, exists_sql, -1, &exists_stmt, nullptr);
    if (rc != SQLITE_OK) {
        valid_sqlite_result(rc, db, "prepare variable exists query");
        return 0;
    }

    rc = sqlite3_bind_text(exists_stmt, 1, variable_name, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        valid_sqlite_result(rc, db, "bind variable exists name");
        sqlite3_finalize(exists_stmt);
        return 0;
    }

    rc = sqlite3_step(exists_stmt);
    if (rc == SQLITE_DONE) {
        log_warn("variable not found: %s", variable_name);
        sqlite3_finalize(exists_stmt);
        return 0;
    }
    if (rc != SQLITE_ROW) {
        valid_sqlite_result(rc, db, "step variable exists query");
        sqlite3_finalize(exists_stmt);
        return 0;
    }
    sqlite3_finalize(exists_stmt);
    return 1;
}
int db_query_variable_info(sqlite3* db, std::vector<db_schema_variable>& res) {
    const char* sql = R"sql(
SELECT v.id,
       v.name
  FROM variable AS v
 ORDER BY v.id ASC;    )sql";

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (!valid_sqlite_result(rc, db, "prepare query variable info")) {
        return 0;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        db_schema_variable varInfo;

        varInfo.id = sqlite3_column_int64(stmt, 0);
        const unsigned char* text = sqlite3_column_text(stmt, 1);
        if (text) {
            varInfo.name = reinterpret_cast<const char*>(text);

        } else {
            log_fatal(
                "Something is terribly wrong. Name was NULL in variable table, which is not "
                "possible since schema has NOT NULL!");
            varInfo.name.clear();
            return 0;
        }
        res.push_back(varInfo);
    }

    if (!valid_sqlite_result(rc, db, "read history rows")) {
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}
int read_variable_history(sqlite3* db, const char* variable_name, MeasurementRecord* record) {
    const char* sql = R"sql(
      SELECT
          d.timestamp,
          d.value
      FROM data AS d
      JOIN variable AS v
      ON v.id = d.variable_id
      WHERE v.name = ?1 AND d.timestamp > ?2
      ORDER BY d.timestamp ASC;
    )sql";
    if (!db_exists_in_database(db, variable_name)) {
        log_warn("variable [%s] doesn't exist in the database.", variable_name);
        return 0;
    }

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (!valid_sqlite_result(rc, db, "prepare history query")) {
        return 0;
    }

    rc = sqlite3_bind_text(stmt, 1, variable_name, -1, SQLITE_TRANSIENT);
    if (!valid_sqlite_result(rc, db, "bind variable name")) {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_int64 last_timestamp = 0;
    auto it = record->find(variable_name);
    if (it != record->end() && !it->second.timestamp.empty()) {
        last_timestamp = it->second.timestamp.back();
    }

    rc = sqlite3_bind_int64(stmt, 2, last_timestamp);
    if (!valid_sqlite_result(rc, db, "bind last timestamp")) {
        sqlite3_finalize(stmt);
        return 0;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        (*record)[variable_name].timestamp.push_back(sqlite3_column_int64(stmt, 0));
        (*record)[variable_name].value.push_back(sqlite3_column_double(stmt, 1));
    }

    if (!valid_sqlite_result(rc, db, "read history rows")) {
        return 0;
    }

    sqlite3_finalize(stmt);

    return 1;
}
int init_test_data_writer(sqlite3* db) {
    const char* sql = R"sql(
      INSERT INTO DataByName (timestamp, variable_name, value)
      VALUES (?1, ?2, ?3);
    )sql";

    int rc = sqlite3_prepare_v2(db, sql, -1, &insert_data_stmt, nullptr);
    return valid_sqlite_result(rc, db, "prepare data insert");
}

void insert_test_value(sqlite3* db, sqlite3_int64 timestamp_ms, const char* name, double value) {
    sqlite3_reset(insert_data_stmt);
    sqlite3_clear_bindings(insert_data_stmt);

    int rc = sqlite3_bind_int64(insert_data_stmt, 1, timestamp_ms);
    valid_sqlite_result(rc, db, "bind timestamp");

    rc = sqlite3_bind_text(insert_data_stmt, 2, name, -1, NULL);
    valid_sqlite_result(rc, db, "bind name");

    rc = sqlite3_bind_double(insert_data_stmt, 3, value);
    valid_sqlite_result(rc, db, "bind value");

    rc = sqlite3_step(insert_data_stmt);

    if (rc != SQLITE_DONE) {
        valid_sqlite_result(rc, db, "insert data");
    }
}
void shutdown_test_data_writer() {
    if (insert_data_stmt != nullptr) {
        sqlite3_finalize(insert_data_stmt);
        insert_data_stmt = nullptr;
    }
}

std::string db_read_sql_file_filtered(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        return {};
    }

    char line[4096];
    std::string result;
    result.reserve(8192);

    while (fgets(line, sizeof(line), fp)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }

        if (p[0] == '-' && p[1] == '-') {
            continue;
        }

        result += p;
    }

    fclose(fp);
    return result;
}
