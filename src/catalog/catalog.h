#pragma once

#include "catalog/tableschema.h"
#include <expected>
#include <vector>

enum class CatalogError {
  FileNotOpen,
  UnvalidTableLineFormat,
  UnvalidColumnLineFormat,
  DuplicateTable
};

struct Catalog {
  std::vector<TableSchema> tables;
  std::expected<void, CatalogError> load();
  void save();
  TableSchema *findTable(const std::string &name);
  std::expected<void, CatalogError> addTable(const TableSchema &schema);
  void dropTable(const std::string &name);
};
