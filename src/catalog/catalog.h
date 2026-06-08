#pragma once

#include "catalog/tableschema.h"
#include <expected>
#include <vector>

enum class CatalogError {
  FileNotOpen,
  UnvalidTableLineFormat,
  UnvalidColumnLineFormat
};

struct Catalog {
  std::vector<TableSchema> tables;
  std::expected<void, CatalogError> load(const std::string &directory);
  void save(const std::string &directory);
  TableSchema *findTable(const std::string &name);
  void addTable(const TableSchema &schema);
  void dropTable(const std::string &name);
};
