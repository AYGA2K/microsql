#pragma once
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
  int textLength; // for TEXT(n); 0 otherwise
  bool notNull;
  bool primaryKey;
};

struct Statement {
  StatementKind kind;
  std::string tableName;

  // --- SELECT ---
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

  // --- DROP TABLE / BEGIN / COMMIT / ROLLBACK ---
  // tableName only
};
