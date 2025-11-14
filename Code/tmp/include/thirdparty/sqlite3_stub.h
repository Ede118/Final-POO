#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef long long sqlite3_int64;
typedef void (*sqlite3_destructor_type)(void*);

#define SQLITE_OK 0
#define SQLITE_ERROR 1
#define SQLITE_ROW 100
#define SQLITE_DONE 101

#define SQLITE_TRANSIENT ((sqlite3_destructor_type)-1)

int sqlite3_open(const char* filename, sqlite3** db);
int sqlite3_close(sqlite3* db);
const char* sqlite3_errmsg(sqlite3* db);
int sqlite3_exec(sqlite3* db, const char* sql,
                 int (*callback)(void*, int, char**, char**),
                 void* data, char** errmsg);
int sqlite3_prepare_v2(sqlite3* db, const char* sql, int nByte,
                       sqlite3_stmt** stmt, const char** tail);
int sqlite3_step(sqlite3_stmt* stmt);
int sqlite3_finalize(sqlite3_stmt* stmt);
int sqlite3_bind_text(sqlite3_stmt* stmt, int index, const char* value, int n,
                      sqlite3_destructor_type destructor);
int sqlite3_bind_int(sqlite3_stmt* stmt, int index, int value);
int sqlite3_bind_int64(sqlite3_stmt* stmt, int index, sqlite3_int64 value);
const unsigned char* sqlite3_column_text(sqlite3_stmt* stmt, int index);
int sqlite3_column_int(sqlite3_stmt* stmt, int index);
sqlite3_int64 sqlite3_column_int64(sqlite3_stmt* stmt, int index);
void sqlite3_free(void* ptr);

#ifdef __cplusplus
}
#endif
