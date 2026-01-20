![intro](media/intro.png)

SQLite/CE: A transactional relational database for Windows CE
-------------------------------------------------------------

**SQLite/CE** is a complete software suite built around a port of SQLite 2.8.17 to Windows CE, allowing Handheld, Palm-size and Pocket PCs running CE 2.0 and later to support full-fledged relational databases and complex queries. SQLite/CE is componentized into separate libraries and executables, meaning it can be used to drive data-driven applications as a standalone DLL or used for end-to-end data management using the native query editor, **SQLite/CEdit**.

# Distribution

SQLite/CE consists of three core binaries: `SQLiteCE.dll`, the ported SQLite library; `SQLiteCEdit.exe`, the native query editor; and `SQLiteCEbench.exe`, the optional benchmarking and validation tool that can be used to test SQLite/CE functionality and performance on your platform.

## Library

The core SQLite/CE library `SQLiteCE.dll` is a fully functional port of SQLite 2.8 to the Windows CE operating system, implementing a serverless, self-contained database engine that supports ACID-compliant transactions and advanced constructs like temporary tables, views, constraints, triggers and indexes. SQLite/CE's SQL interpreter is mostly compliant with the SQL-92 standard and provides most essential features, including support for compound queries and subqueries. For application developers, SQLite/CE carries over SQLite's straightforward C API that can be easily integrated with other applications and redistributed alongside them. 

## SQLite/CEdit

SQLite/CE includes a native query editor, **SQLite/CEdit**, that allows users to design, create, manage and query SQLite 2.x-formatted databases directly from a Windows CE device with a number of notable advanced features. SQLite/CEdit intends to be more than just a Pocket Access alternative, with functionality approaching that of desktop SQL database management tools. 

### Interface

SQLite/CEdit pays considerable attention to workflow efficiency with a goal of implementing "tap-free" paths for most essential operations through universal and context-specific keyboard shortcuts. 

To maximize screen utilization, SQLite/CEdit uses a switchable pane-based UI with three distinct full-screen modes:

#### Query Editor

SQLite/CEdit's query editor pane allows users to prepare and execute queries in-memory or on a database file. The editor supports the following essential features:
- Find/replace with wrap-around search
- Adjustable font scaling
- Line numbering with logical line support
- Flexible execution modes: full script, single-statement or selection
- Ability to abort long-running queries

#### Result Viewer

SQLite/CEdit's result viewer is bimodal and capable of displaying output in either tab-separated plaintext or interactive grid format based on user preference, both of which also provide status feedback on query runtime and rows returned. Users can select and copy individual lines/rows with ease or directly export entire result sets to .txt and .csv formats for use with other applications.

#### Schema Explorer

SQLite/CEdit includes the Schema Explorer, which allows users to graphically explore a database's structure and content through a tree view, with a number of notable features:
- Advanced view mode showing rowcounts and size approximation for individual tables
- Object dropping (with confirmation)
- DDL export for single objects or an entire database schema
- Interactive grid/cell editing for tables for data maintenance

### Configuration and persistence

SQLite/CEdit implements user-configurable options that persist across sessions using the Windows registry, which can also be cleared by the user for uninstallation or reset. Leveraging the same functionality, SQLite/CEdit also remembers recent databases and query files to allow you to quickly resume work or access frequently used scripts and datasets. 

### Data migration

SQLite/CEdit includes tools to migrate data in and out of SQLite databases in a variety of formats:
- Tables export to CSV or SQL `INSERT` statement sets
- Database clone and export to collections of CSVs
- In-memory database instantiation to file
- Import from .CSV and Windows CE database formats with type inference 

## SQLite/CEbench

SQLite/CE includes **SQLite/CEbench**, a benchmarking and validation tool that exercises various features of the SQLite library with detailed timing and categorization. SQLite/CEbench tests across multiple storage paths (memory, object store, storage card) and provides per-category timing breakdowns, device identification, and configurable iteration counts. This application is tailored to the needs of the project's development but is nonetheless provided as something potentially useful to those who wish to validate or benchmark the ported library on their device.

# Installation

SQLite/CE is currently distributed as loose binaries, which can be found in the [project releases](https://github.com/nfinit/SQLiteCE/releases) section by clicking the latest tag and navigating to the end of the description. Release tags can also be used to download the source code snapshot for a specific release.

To install the full SQLite/CE distribution:
- Place `SQLiteCE.dll` in the `\Windows` directory of your device
- Place the binaries `SQLiteCEdit.exe` and `SQLiteCEbench.exe` in a location of your choosing, such as a directory in `\Program Files` or on your storage card.
- Create shortcuts on the desktop and Start menu (`\Windows\Programs`) as desired

# Limitations

SQLite/CE is based on SQLite 2.8, which is quite old (from 2004,) but also much simpler and lighter on memory than newer 3.x releases, while still being much more powerful than the majority of extant database management systems available for Windows CE. But while SQLite/CE can still be very useful, it's important to keep its limitations in mind.

## Comparison to SQLite 3.x

Although essentially similar, SQLite 3.x implements a growing number of new features that are not supported in 2.8. A broad overview of the most significant changes can be found in [this official overview of SQLite 3.0](https://sqlite.org/version3.html), with the most important being:

### Language compatibility

Modern versions of SQLite 3 have introduced many new language enhancements over
the past 20 years. While SQLite 2's SQL implementation is fairly complete, you
will likely encounter limitations from time to time, such as lack of support
for multi-row `INSERT` statements. A more comprehensive exploration of language
limitations (and workarounds) will be published in the future.

### Format compatibility

SQLite 3.x databases **cannot** be read by SQLite 2.8 or SQLite/CE, and vice versa. If you wish to directly work with database files created on your handheld on another system with a more modern version of SQLite, you must export the data and re-import it to a new SQLite 3.x database. SQLite/CEdit has a number of features that can make this process less painful, with the ability to export entire databases into collections of CSVs, as well as extract complete object definitions (DDL) that can then be loaded into a new database.

### BLOB support

SQLite 2.8 stores *all* data internally as ASCII text, including Binary Large Objects (BLOBs). This approach, while officially supported, requires developers working with BLOB data to explicitly encode and decode it to and from SQLite's BLOB format before it can be stored or read, using the API functions `sqlite_encode_binary()` and `sqlite_decode_binary()`, respectively. Compared to SQLite 3's fully native BLOB support, the SQLite 2 encoded BLOB format is slightly more inefficient, and can incur a 1-2% storage penalty.

### Collation

SQLite 2.8 does not support user-defined collation for sorting and comparison,
and always performs these operations using byte-order comparison, which does
not allow for locale-aware (e.g. international characters) or case-insensitive
comparison.

### Unicode support

SQLite 2.8 is capable of storing and operating on UTF-8 characters, but is
not itself encoding-aware, instead treating everything as single-byte ASCII text
in all cases. This causes unpredictable behavior with core SQLite components
such as sorting, string (e.g. `upper()`, `lower()`) and pattern matching functions
(such as the SQL `LIKE` operator) that will often fail to work consistently with unicode characters.
For this reason, SQLite/CE databases should be treated as strictly ASCII-only in
almost all cases.

### Rowcount and size limits

Though still far beyond the capabilities of most Windows CE devices, SQLite 2.8
has hard limits on rowcounts and database sizes due to its reliance on 32-bit
signed integers to identify rows and data pages. A single table can store a
theoretical maximum of around 2,147,483,647 rows, and single databases are 
limited to ~ 2 GB in size.

### Concurrency

SQLite 2.8 can support multiple readers or one writer, but not both at the same
time as write operations require database-level locking, meaning prolonged 
write operations can cause significant contention with consuming applications.

## Limitations of Windows CE

### Concurrency

Under Windows CE 2.0, SQLite/CE can only support *one user at any time*, due to
lacking `LockFileEx()` which wasn't introduced until CE 2.11. Systems running
CE 2.11 and later (HPC Pro, later Palm-sized PCs, HPC 2000 and Pocket PCs) are
subject to standard SQLite 2.8 concurrency limitations when running a 2.11+
targeted build. 

### Console

Although later versions of Windows CE ship with a primitive command shell,
SQLite/CE does not assume its presence or support it, and thus does not implement
a command-line client. All database management and queries must be executed through
applications using the library's C API or through SQLite/CEdit. Debug output
for developers can be handled using `OutputDebugStringW()`.

### Path limits

Like standard Windows, Windows CE has an upper limit of 260 characters for
file paths.

### Memory constraints

While not a specific SQLite limitation, working with large datasets, even from
storage cards, can easily exceed the small memories of most Windows CE devices.
Design queries accordingly.

# Notes and acknowledgements

SQLite/CE is based on SQLite 2.8.17, created by D. Richard Hipp and released to
the public domain.

## Generative AI 

The SQLite/CE port and front-end applications have been developed with significant 
AI assistance, primarily from Claude Opus 4.5. While each new feature and change
is fairly well-tested after implementation and during daily use, be aware that
this codebase may have some strange patterns or bugs littered throughout it. The
core SQLite source (located at `src/sqlite` in this repository) remains original
to the official SQLite 2.8 source distribution, while modifications specific
to the port can be found at `src/sqlite-ce`. Thus far, the core port has
proven remarkably reliable for most common use, aside from some early memory
alignment bugs that hampered testing, but always be aware!

## Tooling and development

The SQLite/CE suite is built and tested under Windows NT 4.0 and Visual C++ 6.0
with the relevant Windows CE SDKs. Project files and setup instructions will
be provided as SQLite/CE begins moving towards targeting Windows CE 2.11 
and platform-specific configuration for all architectures are solidified. 

## Licensing

In the spirit of SQLite's own licensing practice, the library port sources at 
`src/sqlite-ce` are also released to the public domain. The frontend 
applications remain under a BSD 2-clause license.

For full details, please see the project [LICENSE file](https://github.com/nfinit/SQLiteCE/blob/main/LICENSE).
