/*
** SQLITE AMALGAMATION PLACEHOLDER
** ─────────────────────────────────────────────────────────────────────────────
** The real sqlite3.h is ~12 000 lines. This stub contains ONLY the symbols
** used by this project so it compiles without network access.
**
** To use the real SQLite (recommended for production):
**   1. Download https://www.sqlite.org/download.html  → "amalgamation" zip
**   2. Replace this file AND sqlite3.c with the real ones from that zip.
** ─────────────────────────────────────────────────────────────────────────────
*/
#ifndef SQLITE3_H
#define SQLITE3_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>   /* size_t */
#include <stdint.h>   /* int64_t */

/* ── Return codes ─────────────────────────────────────────────────────────── */
#define SQLITE_OK          0
#define SQLITE_ERROR       1
#define SQLITE_BUSY        5
#define SQLITE_ROW        100
#define SQLITE_DONE       101

/* ── Open flags ───────────────────────────────────────────────────────────── */
#define SQLITE_OPEN_READWRITE  0x00000002
#define SQLITE_OPEN_CREATE     0x00000004
#define SQLITE_OPEN_FULLMUTEX  0x00010000

/* ── Types ────────────────────────────────────────────────────────────────── */
typedef struct sqlite3       sqlite3;
typedef struct sqlite3_stmt  sqlite3_stmt;
typedef int64_t              sqlite3_int64;

/* ── Core API ─────────────────────────────────────────────────────────────── */
int   sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs);
int   sqlite3_close(sqlite3 *db);
int   sqlite3_exec(sqlite3 *db, const char *sql, int(*cb)(void*,int,char**,char**), void *arg, char **errmsg);
void  sqlite3_free(void *ptr);
const char *sqlite3_errmsg(sqlite3 *db);
int   sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail);
int   sqlite3_step(sqlite3_stmt *pStmt);
int   sqlite3_finalize(sqlite3_stmt *pStmt);
int   sqlite3_reset(sqlite3_stmt *pStmt);
int   sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void(*)(void*));
int   sqlite3_bind_int(sqlite3_stmt*, int, int);
int   sqlite3_bind_int64(sqlite3_stmt*, int, sqlite3_int64);
int   sqlite3_bind_double(sqlite3_stmt*, int, double);
int   sqlite3_column_int(sqlite3_stmt*, int iCol);
sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int iCol);
double sqlite3_column_double(sqlite3_stmt*, int iCol);
const unsigned char *sqlite3_column_text(sqlite3_stmt*, int iCol);
int   sqlite3_column_count(sqlite3_stmt *pStmt);

#define SQLITE_TRANSIENT  ((void(*)(void*))(-1))
#define SQLITE_STATIC     ((void(*)(void*))(0))

#ifdef __cplusplus
}
#endif
#endif /* SQLITE3_H */
