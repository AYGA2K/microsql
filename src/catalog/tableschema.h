#pragma once

#include "ast/statement.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>
struct TableSchema {
  std::string tableName;
  std::vector<ColumnDefinition> columns;
  std::string filePath;
  size_t rowSize();
  uint16_t columnOffset(int index);
  int column_index(const std::string &name);
};
