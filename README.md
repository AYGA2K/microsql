# microsql

SQL database engine written from scratch in C++23 with no external libraries. Has its own lexer, parser, and executor, supports CREATE, INSERT, SELECT with WHERE, UPDATE, and DELETE, and stores data in a custom binary format using 4 KB slotted pages.

## How it works

**Lexer:** — turns the raw SQL string into a list of tokens.

**Parser:** — takes the tokens and produces an AST made of a `Statement` and a list of `Expression`s.

**Executor:** — walks the AST and runs the query. For reads it scans pages, evaluates the WHERE clause per row, and builds the result. For writes it serializes the row and inserts or updates it — updates are in place if the row fits, otherwise the new row is written at the free pointer.

**Storage:** — each table is a binary `.ms` file split into 4096-byte slotted pages. The slot directory grows down from the header, row data grows up from the end, and free space sits in the middle.

```
+--------------------+  0
|   header (16B)     |  page_id, num_slots, free_space_ptr, flags
+--------------------+  16
|   slot 0 (4B)      |  offset + length of row 0
|   slot 1 (4B)      |
|   ...              |
+--------------------+
|                    |
|    free space      |
|                    |
+--------------------+
|   ...              |
|   row 1            |
|   row 0            |
+--------------------+  4096
```

Row encoding: `INTEGER` and `FLOAT` are 8 bytes, `BOOLEAN` is 1 byte. `TEXT(n)` is stored as a 2-byte length prefix followed by the actual string bytes, so shorter strings take less space. If an update makes a row larger than the original, the new row is written at the free pointer and the slot is updated to point to it.

**Catalog** — stores table schemas in a `catalog.txt` file. It is written on every `CREATE TABLE` or `DROP TABLE` and loaded on startup.

## Supported SQL

```sql
CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT(100) NOT NULL, age INTEGER);
INSERT INTO users VALUES (1, 'alice', 30);
SELECT * FROM users WHERE age > 25;
UPDATE users SET age = 31 WHERE id = 1;
DELETE FROM users WHERE id = 1;
DROP TABLE users;
```

Data types: `INTEGER`, `FLOAT`, `TEXT(n)`, `BOOLEAN`

## Build

Requires CMake 3.16+ and a C++23-capable compiler (GCC 14 / Clang 17+).

```sh
make config
make build
make run      # launch the REPL
make test
```

## Usage

```
>>> CREATE TABLE t (id INTEGER, val TEXT(50));
>>> INSERT INTO t VALUES (1, 'hello');
>>> SELECT * FROM t;
+----+-------+
| id | val   |
+----+-------+
| 1  | hello |
+----+-------+
>>> exit;
```
