#pragma once
#include "sqlite3.h"

typedef struct DbContext {
    char* dbName = nullptr;
    sqlite3* db = nullptr;

} DbContext;

int db_connect(DbContext* ctx);
