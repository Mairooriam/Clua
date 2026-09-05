
#include "db_access.h"

#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/allocator.h"
#include "core/log.h"
#include "core/nob.h"
#include "sqlite3.h"

int db_connect(DbContext* ctx, const char* dbName, char* schema) {
    // TODO: handle assert probably not needed
    NOB_ASSERT(!ctx->handle && "hanlde already intialized. call without already init");
    NOB_ASSERT(dbName && "dbName supplied was invalid");
    NOB_ASSERT(schema && "schema supplied was invalid");

    int sql_rc = sqlite3_open(dbName, &ctx->handle);
    if (!valid_sqlite_result(sql_rc, ctx->handle, "db_connect")) {
        if (ctx->handle) sqlite3_close(ctx->handle);
        return 0;
    }

    sql_rc = sqlite3_exec(ctx->handle, schema, NULL, NULL, NULL);
    if (!valid_sqlite_result(sql_rc, ctx->handle, "db_connect")) {
        if (ctx->handle) sqlite3_close(ctx->handle);
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
    sqlite3_stmt* exists_stmt = NULL;
    const char* exists_sql = "SELECT 1 FROM variable WHERE name = ?1 LIMIT 1;";
    int rc = sqlite3_prepare_v2(db, exists_sql, -1, &exists_stmt, NULL);
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
arr_db_schema_variables* db_query_available_variables(sqlite3* db, memory_arena* arena) {
    int64_t count = 0;
    {
        sqlite3_stmt* count_stmt = NULL;
        int rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM variable;", -1, &count_stmt, NULL);
        if (!valid_sqlite_result(rc, db, "prepare variable count")) return NULL;
        if (sqlite3_step(count_stmt) == SQLITE_ROW) count = sqlite3_column_int64(count_stmt, 0);
        sqlite3_finalize(count_stmt);
    }
    if (count <= 0) {
        log_warn("No avaiable variables in db!");
        return NULL;
    }
    const char* sql =
        "SELECT v.id, v.name "
        "FROM variable AS v ORDER BY v.id ASC ";

    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (!valid_sqlite_result(rc, db, "prepare query variable info")) {
        return 0;
    }

    arr_db_schema_variables* result = (arr_db_schema_variables*)arena_alloc(
        arena, sizeof(arr_db_schema_variables), alignof(arr_db_schema_variables));
    result->items = (Db_schema_variable*)arena_alloc(
        arena, sizeof(Db_schema_variable) * (size_t)count, alignof(Db_schema_variable));
    result->count = 0;
    result->capacity = (size_t)count;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Db_schema_variable varInfo;

        varInfo.id = (uint64_t)sqlite3_column_int64(stmt, 0);
        const char* text = (const char*)sqlite3_column_text(stmt, 1);
        if (text) {
            varInfo.name = arena_strdup(arena, text, alignof(char));
        } else {
            log_fatal(
                "Something is terribly wrong. Name was NULL in variable table, which is not "
                "possible since schema has NOT NULL!");
            return 0;
            varInfo.name = NULL;
        }
        result->items[result->count++] = varInfo;
    }

    if (!valid_sqlite_result(rc, db, "read history rows")) {
        return NULL;
    }

    sqlite3_finalize(stmt);
    return result;
}

// TODO: currently lets user define how big meas is and will let user to
//  access out of bounds
//  also just raw post increment array access
int db_read_variable_history(sqlite3* db, Measurement* meas) {
    static const char SQL_READ_HISTORY[] =
        "SELECT d.timestamp, d.value\n"
        "FROM data AS d\n"
        "JOIN variable AS v ON v.id = d.variable_id\n"
        "WHERE v.name = ?1 AND d.timestamp > ?2\n"
        "ORDER BY d.timestamp ASC;";

    if (!db_exists_in_database(db, meas->name)) {
        log_warn("variable [%s] doesn't exist in the database.", meas->name);
        return 0;
    }

    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2(db, SQL_READ_HISTORY, -1, &stmt, NULL);
    if (!valid_sqlite_result(rc, db, "prepare history query")) {
        return 0;
    }

    rc = sqlite3_bind_text(stmt, 1, meas->name, -1, SQLITE_TRANSIENT);
    if (!valid_sqlite_result(rc, db, "bind variable name")) {
        sqlite3_finalize(stmt);
        return 0;
    }
    int64_t lastTimestamp = 0;
    if (meas->data.timestamp.count != 0) {
        lastTimestamp = nob_da_last(&meas->data.timestamp);
    }

    rc = sqlite3_bind_int64(stmt, 2, lastTimestamp);
    if (!valid_sqlite_result(rc, db, "bind last timestamp")) {
        sqlite3_finalize(stmt);
        return 0;
    }

    // arr_DataPoints* meas =
    //     (arr_DataPoints*)arena_alloc(arena, sizeof(arr_DataPoints), alignof(arr_DataPoints));
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int64_t timestamp = (int64_t)sqlite3_column_int64(stmt, 0);
        float value = (float)sqlite3_column_double(stmt, 1);
        meas->data.timestamp.items[meas->data.timestamp.count++] = timestamp;
        meas->data.value.items[meas->data.value.count++] = value;
    }

    if (!valid_sqlite_result(rc, db, "read history rows")) {
        return 0;
    }

    sqlite3_finalize(stmt);

    return 1;
}

void db_write(DbContext* ctx, sqlite3_int64 timestamp_ms, const char* name, double value) {
    sqlite3_reset(ctx->insert_data_stmt);
    sqlite3_clear_bindings(ctx->insert_data_stmt);

    int rc = sqlite3_bind_int64(ctx->insert_data_stmt, 1, timestamp_ms);
    valid_sqlite_result(rc, ctx->handle, "bind timestamp");

    rc = sqlite3_bind_text(ctx->insert_data_stmt, 2, name, -1, NULL);
    valid_sqlite_result(rc, ctx->handle, "bind name");

    rc = sqlite3_bind_double(ctx->insert_data_stmt, 3, value);
    valid_sqlite_result(rc, ctx->handle, "bind value");

    rc = sqlite3_step(ctx->insert_data_stmt);

    if (rc != SQLITE_DONE) {
        valid_sqlite_result(rc, ctx->handle, "insert data");
    }
}
int db_write_begin(DbContext* ctx) {
    static const char sql[] =
        "INSERT INTO DataByName (timestamp, variable_name, value)\n"
        "VALUES (?1, ?2, ?3);\n";

    int rc = sqlite3_prepare_v2(ctx->handle, sql, -1, &ctx->insert_data_stmt, NULL);
    return valid_sqlite_result(rc, ctx->handle, "prepare data insert");
}
void db_write_end(DbContext* ctx) {
    if (ctx->insert_data_stmt != NULL) {
        sqlite3_finalize(ctx->insert_data_stmt);
        ctx->insert_data_stmt = NULL;
    }
}

char* db_read_sql_schema(const char* schemaFilename, memory_arena* arena) {
    struct stat st;

    // TODO: make fs utility that chekcs file size safely first checking if its a dir. etc.
    if (stat(schemaFilename, &st) != 0) {
        int saved = errno;
        char errMsg[256];
        strerror_r(saved, errMsg, sizeof errMsg);
        log_error("stat('%s') failed: %s", schemaFilename, errMsg);
        return NULL;
    }

    if (S_ISDIR(st.st_mode)) {
        log_error("schema path is a directory: %s", schemaFilename);
        return NULL;
    }

    FILE* fp = fopen(schemaFilename, "r");
    if (!fp) {
        log_error("Reading file: %s");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    if (file_size <= 0) {
        fclose(fp);
        log_warn("File at: %s is empty.");
        return NULL;
    } else if (file_size == -1L) {
    }

    char* buf = (char*)arena_alloc(arena, (size_t)file_size + 1, alignof(char));
    if (!buf) {
        fclose(fp);
        log_fatal("Allocating buffer failed.");
        return NULL;
    }

    char line[KB(1)];
    size_t written = 0;

    while (fgets(line, sizeof(line), fp)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (p[0] == '-' && p[1] == '-') continue;
        size_t len = strlen(p);
        memcpy(buf + written, p, len);
        written += len;
    }

    buf[written] = '\0';
    fclose(fp);
    return buf;
}
void db_context_init(DbContext* ctx) {
    memset(ctx, 0, sizeof(DbContext));
}
arr_f32* arr_f32_create_in_arena(memory_arena* arena, size_t count) {
    arr_f32* result = (arr_f32*)arena_alloc(arena, sizeof(arr_f32), alignof(arr_f32));
    result->count = 0;
    result->capacity = count;
    result->items = (float*)arena_alloc(arena, sizeof(float) * count, alignof(float));
    return result;
}

arr_i64* arr_i64_create_in_arena(memory_arena* arena, size_t count) {
    arr_i64* result = (arr_i64*)arena_alloc(arena, sizeof(arr_i64), alignof(arr_i64));
    result->count = 0;
    result->capacity = count;
    result->items = (int64_t*)arena_alloc(arena, sizeof(int64_t) * count, alignof(int64_t));
    return result;
}

arr_DataPoints* arr_datapoints_create_in_arena(memory_arena* arena, size_t count) {
    arr_DataPoints* result =
        (arr_DataPoints*)arena_alloc(arena, sizeof(arr_DataPoints), alignof(arr_DataPoints));
    result->timestamp = *arr_i64_create_in_arena(arena, count);
    result->value = *arr_f32_create_in_arena(arena, count);
    return result;
}
Measurement* measurement_create_in_arena(
    memory_arena* arena, char* strData, size_t strLenght, size_t count) {
    Measurement* meas = (Measurement*)arena_alloc(arena, sizeof(Measurement), alignof(Measurement));
    meas->arena = arena;
    meas->name = (char*)arena_alloc(arena, strLenght + 1, alignof(char));
    memcpy(meas->name, strData, strLenght);
    meas->name[strLenght] = '\0';
    meas->data.timestamp = *arr_i64_create_in_arena(arena, count);
    meas->data.value = *arr_f32_create_in_arena(arena, count);
    return meas;
}
