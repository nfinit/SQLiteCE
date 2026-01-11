![intro](media/intro.png)

SQLite/CE: A transactional, relational database engine for Windows CE
---------------------------------------------------------------------

**SQLite/CE** is a port of SQLite 2.8.17 to Windows CE 2.0 and later, allowing these early handhelds to support complex relational databases and SQL queries for any use case. SQLite/CE can power data-driven applications on your device as a standalone DLL or can be used to directly manage personal databases with, **SQLite/CEdit** the bundled advanced query editor. The complete SQLite/CE package currently targets only Handheld PCs (including 480x240 devices,) but Palm-size and Pocket PC support is planned in future releases.

SQLite/CE's components, particularly the graphical front-end applications in the distribution, were written with the assistance of Claude Opus 4.5. While the process was heavily guided and managed, be aware that there could still be strange bugs or oversights lurking within the code of these pre-release versions. A full human rewrite is planned in the future once the full feature set is cemented and implemented.

# Overview
The standard SQLite/CE distribution is a set of three applications: the core library `SQLiteCE.dll`, and two front-end applications; the advanced query editor **SQLite/CEdit** and the **SQLite/CE Test** harness. SQLite/CE's front-end applications currently only support Handheld PCs (including 480x240 devices,) support for Palm-sized and Pocket PCs is planned in future releases.

## Library
The SQLite/CE library is a full-fledged port of SQLite to the Windows CE operating system, supporting ACID-compliant transactions and constructs such as temporary tables, constraints, triggers and indexes. SQLite/CE's SQL interpreter has full SQL-92 support with the ability to handle complex queries including compound queries and subqueries. Like standard SQLite, the SQLite/CE library implements a simple C API that can be easily used by front-end applications.

## Query editor
**SQLite/CEdit** is an advanced query editor that allows users to design, create, manage and query SQLite 2.x databases directly on their devices. SQLite/CEdit maximizes screen utilization by splitting query and result views into distinct "panes" that can be switched between by graphical controls or keyboard shortcuts, with results displayed either as exportable plaintext or (in future releases) interactive grids. SQLite/CEdit in its current form is mainly intended for technical users as a database design and testing tool.

SQLite/CEdit executes multi-line queries statement-by-statement and supports various modes of execution; all at once, only highlighted text, or only the statement currently under the cursor position, allowing users to bundle multiple distinct queries into a single file and run only what they need. The editor interface is designed to maximize convenience in workflow for many different kinds of user, with an interface that is both graphical and keyboard-driven depending on preference. Most major functions have keyboard shortcuts implemented. 

SQLite/CEdit can be used to import and export data from CSV format and can even import data from Windows CE in-memory databases. The editor's CSV export tools are capable of exporting individual tables or entire databases as collections of multiple CSVs.

## Test harness
SQLite/CE includes a simple "test harness" application that exercises various functions of the SQLite/CE core library. This application is mainly used and maintained for developer debugging, but is included in the distribution as a demonstration of SQLite's more advanced capabilities that also allows users to validate untested systems.

# Installation
SQLite/CE is currently distributed in loose binary form and its components must be manually copied and placed on your device:
- `SQLiteCE.dll` must reside in the `\Windows` directory on the handheld
- `SQLiteCEdit.exe` and `SQLiteCETest.exe` can be placed anywhere to user preference

# Limitations (what SQLite/CE is not, and cannot do)
## SQLite 2.x is not 3.x
SQLite/CE is a port of SQLite 2.8, which was chosen both for its relative simplicity and also smaller memory footprint for CE devices, which often have 8 MB or less of available memory for applications. While this has allowed for SQLite/CE to achieve its goals, it comes with a number of limitations:
- **No multi-row `INSERT`S**: Each `INSERT` must be a distinct statement.
- **No support for adding columns to tables after creation** using `ALTER TABLE ADD COLUMN`. Adding or removing columns from a table requires you to create a new table and manually migrate data.
- **No `IF EXISTS`:** Checking if an object exists must be done by the external application by querying the `sqlite_master` table.
- **No common table expressions** such as `WITH`
- **No window functions** such as `OVER` or `PARTITION BY`
- **No JSON functions or full-text search**
- **No user-defined collation**
- **Concurrency:** SQLite 2.x supports multiple readers **OR** a single writer, but not both at the same time, unlike SQLite 3.x. Windows CE itself introduces further concurrency limitations that are discussed in the next section.
- **Database compatibility:** SQLite 2.x cannot work with databases created by newer versions of SQLite, as 3.x significantly overhauled the SQLite database file format. If you would like to work with SQLite 3.x databases on your handheld, you will need to dump and reimport the data to a 2.x-formatted database on another system first. 
- **Encoded BLOBs:** SQLite 2.x officially supports unstructured Binary Large Objects (BLOBs) as a data type, but very inefficiently as it stores all data internally as ASCII text, which requires BLOBs to be encoded and decoded from ASCII during operation. Because of this, SQLite 2.x BLOBs incur slight overhead, and cannot be directly operated on in queries and comparisons.
- **Unicode support:** SQLite 2.x is not encoding-aware and effectively ASCII-only due to unpredictable behavior of its string and sorting functions with accented and other UTF-8 characters, though it may still technically be able to store them.
- **Row limits:** SQLite 2.x stores ROWIDs as signed 32-bit integers, meaning each table can "only" store up ~ 2,147,483,647 unique rows, which will cause `INSERT` operations to fail after that limit is reached. Given the memory limitations of most CE devices, however, this is merely a theoretical ceiling.
- **Database size limits:** SQLite 2.x uses 32-bit signed integers to identify data pages, putting an upper limit of ~2 GB on database size, which could be potentially limiting on higher-end CE devices with large storage cards.

## Windows CE is not Windows NT
Windows CE, while incredibly advanced for a 1990s embedded operating system, has a number of fundamental limitations that impact how SQLite/CE can function. Some key limitations include:
- **Concurrency (CE 2.0 only):** Prior to the introduction of `LockFileEx()` in Windows CE 2.11, Windows CE was not capable of supporting shared files among multiple applications, even read-only usage. To protect against corruption, SQLite/CE for CE 2.0 devices instead implements a primitive no-op locking scheme, and allows only one application to access the database at a time.
- **File handling**: Windows CE has a path limit of 260 characters maximum.
