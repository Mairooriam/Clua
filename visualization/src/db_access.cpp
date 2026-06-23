
#include "db_access.h"

#include "log.h"
#include "sqlite3.h"

int db_connect(DbContext* ctx) {
    if (!ctx->dbName) {
        log_error("DbContext is missing name");
    }

    int sql_rc = sqlite3_open_v2(ctx->dbName, &ctx->db);
}
