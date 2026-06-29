#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class StatementKind {
  SELECT,
  INSERT,
  UPDATE,
  DELETE,
  CREATE_TABLE,
  DROP_TABLE,
  CREATE_INDEX,
  BEGIN,
  COMMIT,
  ROLLBACK,
};
enum class DataType { INTEGER, FLOAT, TEXT, BOOLEAN };

struct ColumnDefinition {
  std::string name;
  DataType type;
  int textLength; // for TEXT(n); defaults to 255 if no length specified
  bool notNull = false;
  bool primaryKey = false;

  uint16_t size() const {
    switch (type) {
    case DataType::BOOLEAN: return 1;
    case DataType::INTEGER: return 8;
    case DataType::FLOAT:   return 8;
    case DataType::TEXT:    return static_cast<uint16_t>(textLength);
    }
    return 0;
  }
};

struct Statement {
  StatementKind kind;
  std::string tableName;

  // --- SELECT ---
  // selectColumns: indices into ParseResult.expressions for each output
  // column. Empty means SELECT * was used.
  std::vector<int> selectColumns;
  int whereIndex; // index into expressions; -1 = no WHERE clause

  // --- INSERT ---
  std::vector<std::string> insertColumnNames; // optional; empty = all columns
  std::vector<int> insertValues; // expression indices for VALUES list

  // --- UPDATE ---
  // assignments: each pair is (columnName, expressionIndex)
  std::vector<std::pair<std::string, int>> assignments;
  // where_index also used here

  // --- DELETE ---
  // tableName + whereIndex

  // --- CREATE TABLE ---
  std::vector<ColumnDefinition> columnDefinitions;

  // --- CREATE INDEX ---
  std::string indexColumnName;

  // --- DROP TABLE ---
  // tableName only
};
