/*
** SQLiteCEdit - Database API Interface
**
** This header defines a clean interface for database operations,
** separating database logic from UI concerns.
**
** Design Goals:
** - No UI dependencies (MessageBox, window handles, etc.)
** - Callback-based progress and error reporting
** - Reusable without UI code
**
** Usage Pattern:
**   DbResult result;
**   DbQuery query = { sql, strlen(sql), NULL, NULL };
**   int rc = DbExecuteQuery(&query, &result, MyCallback, userData);
**   if (rc != DB_OK) {
**       // Handle error using result.errorMsg
**   }
**   DbFreeResult(&result);
**
** NOTE: This is a recommended interface. Current implementation in
** execute.c and database.c can be gradually migrated to use this API.
*/

#ifndef SQLITE_CE_DB_API_H
#define SQLITE_CE_DB_API_H

#include "sqlite.h"

/*============================================================================
** Return Codes
**============================================================================*/

#define DB_OK           0   /* Success */
#define DB_ERROR        1   /* General error */
#define DB_BUSY         2   /* Database is locked */
#define DB_NOMEM        3   /* Out of memory */
#define DB_IOERR        4   /* I/O error */
#define DB_CORRUPT      5   /* Database corruption */
#define DB_NOTFOUND     6   /* Table/column not found */
#define DB_CONSTRAINT   7   /* Constraint violation */
#define DB_ABORT        8   /* Query aborted by callback */

/*============================================================================
** Callback Types
**============================================================================*/

/*
** Progress callback - called periodically during long operations.
** Return non-zero to abort the operation.
*/
typedef int (*DbProgressCallback)(void *userData, int percentComplete);

/*
** Row callback - called for each result row.
** Return non-zero to stop iteration.
*/
typedef int (*DbRowCallback)(void *userData, int argc, char **argv, char **colNames);

/*
** Error callback - called when an error occurs.
** Allows UI to display error without DB layer knowing about UI.
*/
typedef void (*DbErrorCallback)(void *userData, int errCode, const char *errMsg);

/*============================================================================
** Data Structures
**============================================================================*/

/*
** Query input structure.
*/
typedef struct DbQuery {
    const char *sql;            /* SQL statement */
    int sqlLen;                 /* Length of SQL (-1 for null-terminated) */
    DbProgressCallback progress; /* Optional progress callback */
    void *progressData;         /* User data for progress callback */
} DbQuery;

/*
** Query result structure.
*/
typedef struct DbResult {
    int returnCode;             /* DB_OK or error code */
    char *errorMsg;             /* Error message (caller must free) */
    int rowsAffected;           /* Number of rows affected by INSERT/UPDATE/DELETE */
    int64 lastInsertId;         /* Last insert rowid */
    double executionTime;       /* Execution time in seconds */

    /* Result set (for SELECT queries) */
    int columnCount;            /* Number of columns */
    int rowCount;               /* Number of rows */
    char **columnNames;         /* Column names (columnCount entries) */
    char ***rows;               /* Row data (rowCount x columnCount) */
} DbResult;

/*
** Table metadata structure.
*/
typedef struct DbTableInfo {
    char *name;                 /* Table name */
    int columnCount;            /* Number of columns */
    char **columnNames;         /* Column names */
    char **columnTypes;         /* Column type affinities */
    int *notNull;               /* NOT NULL constraints */
    int *primaryKey;            /* Primary key columns */
    int rowCount;               /* Approximate row count */
} DbTableInfo;

/*============================================================================
** Database Operations
**============================================================================*/

/*
** Execute a query and optionally collect results.
** For SELECT queries, results are stored in result->rows.
** For other queries, only rowsAffected is populated.
**
** Parameters:
**   db       - SQLite database handle
**   query    - Query input (SQL and optional progress callback)
**   result   - Output result structure (caller allocates, DbFreeResult to free)
**   rowCb    - Optional callback for streaming row processing
**   userData - User data passed to row callback
**
** Returns DB_OK on success, error code on failure.
*/
int DbExecuteQuery(sqlite *db, const DbQuery *query, DbResult *result,
                   DbRowCallback rowCb, void *userData);

/*
** Free result structure contents.
** Does not free the DbResult struct itself.
*/
void DbFreeResult(DbResult *result);

/*
** Get table metadata.
** Caller must call DbFreeTableInfo to free.
*/
int DbGetTableInfo(sqlite *db, const char *tableName, DbTableInfo *info);

/*
** Free table info structure contents.
*/
void DbFreeTableInfo(DbTableInfo *info);

/*
** Get list of tables in database.
** Returns allocated array of table names (caller must free each and array).
*/
int DbGetTableList(sqlite *db, char ***tables, int *count);

/*
** Begin/commit/rollback transaction.
*/
int DbBeginTransaction(sqlite *db);
int DbCommitTransaction(sqlite *db);
int DbRollbackTransaction(sqlite *db);

/*============================================================================
** Error Handling
**============================================================================*/

/*
** Set global error callback.
** This allows the UI layer to receive error notifications without
** the database layer depending on UI code.
*/
void DbSetErrorCallback(DbErrorCallback callback, void *userData);

/*
** Convert error code to string.
*/
const char *DbErrorString(int errCode);

#endif /* SQLITE_CE_DB_API_H */
