
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
    if (!check_sqlite_result(sql_rc, ctx->db, "db_connect")) {
        if (ctx->db) sqlite3_close(ctx->db);
        return 0;
    }

    sql_rc = sqlite3_exec(ctx->db, sql.c_str(), nullptr, nullptr, nullptr);
    if (!check_sqlite_result(sql_rc, ctx->db, "db_connect")) {
        if (ctx->db) sqlite3_close(ctx->db);
        return 0;
    }
    return 1;
}

int check_sqlite_result(int rc, sqlite3* db, const char* context) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        log_error("[sqlite] %s failed: %s\n", context, db ? sqlite3_errmsg(db) : "unknown");
        return 0;
    } else {
        return 1;
    }
}
int read_variable_history(sqlite3* db, const char* variable_name, MeasurementRecord* record) {
    const char* sql = R"sql(
        SELECT
            d.timestamp,
            d.value
        FROM data AS d
        JOIN variable AS v
            ON v.id = d.variable_id
        WHERE v.name = ?1
        ORDER BY d.timestamp ASC;
    )sql";

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        check_sqlite_result(rc, db, "prepare history query");
        return 0;
    }

    rc = sqlite3_bind_text(stmt, 1, variable_name, -1, SQLITE_TRANSIENT);

    if (rc != SQLITE_OK) {
        check_sqlite_result(rc, db, "bind variable name");
        sqlite3_finalize(stmt);
        return 0;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        (*record)[variable_name].timestamp.push_back(sqlite3_column_int64(stmt, 0));
        (*record)[variable_name].value.push_back(sqlite3_column_double(stmt, 1));
    }

    if (rc != SQLITE_DONE) {
        check_sqlite_result(rc, db, "read history rows");
        return 0;
    }

    sqlite3_finalize(stmt);

    return 1;
}
int init_test_data_writer(sqlite3* db) {
    const char* sql = R"sql(
        INSERT INTO data (timestamp, variable_id, value)
        VALUES (?1, ?2, ?3);
    )sql";

    int rc = sqlite3_prepare_v2(db, sql, -1, &insert_data_stmt, nullptr);
    return check_sqlite_result(rc, db, "prepare data insert");
}

void insert_test_value(sqlite3* db, sqlite3_int64 timestamp_ms, int variable_id, double value) {
    sqlite3_reset(insert_data_stmt);
    sqlite3_clear_bindings(insert_data_stmt);

    int rc = sqlite3_bind_int64(insert_data_stmt, 1, timestamp_ms);
    check_sqlite_result(rc, db, "bind timestamp");

    rc = sqlite3_bind_int(insert_data_stmt, 2, variable_id);
    check_sqlite_result(rc, db, "bind variable_id");

    rc = sqlite3_bind_double(insert_data_stmt, 3, value);
    check_sqlite_result(rc, db, "bind value");

    rc = sqlite3_step(insert_data_stmt);

    if (rc != SQLITE_DONE) {
        check_sqlite_result(rc, db, "insert data");
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
