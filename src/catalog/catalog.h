#pragma once

#include "catalog/tableschema.h"
#include <expected>
#include <vector>

enum class CatalogError {
  FileNotOpen,
  UnvalidTableLineFormat,
  UnvalidColumnLineFormat,
  DuplicateTable,
  TableNotFound,
};

struct Catalog {
  std::vector<TableSchema> tables;
  std::expected<void, CatalogError> load();
  void save();
  TableSchema *findTable(const std::string &name);
  std::expected<void, CatalogError> addTable(const TableSchema &schema);
  std::expected<void, CatalogError> dropTable(const std::string &name);
};
