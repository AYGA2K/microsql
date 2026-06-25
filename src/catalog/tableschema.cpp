#include "tableschema.h"
#include "ast/statement.h"
#include <cstddef>
#include <cstdint>

size_t TableSchema::rowSize() {
  size_t size = 0;
  for (auto const &column : this->columns) {
    size += column.size();
  }
  return size;
}

uint16_t TableSchema::columnOffset(int index) {
  uint16_t offset = 0;
  for (int i = 0; i < index && i < static_cast<int>(this->columns.size()); i++) {
    offset += this->columns[i].size();
  }
  return offset;
}

int TableSchema::column_index(const std::string &name) {
  for (int i = 0; i < static_cast<int>(this->columns.size()); i++) {
    if (this->columns[i].name == name)
      return i;
  }
  return -1;
}

std::vector<std::string> TableSchema::columnNames() {
  std::vector<std::string> names;
  for (auto const &column : this->columns) {
    names.push_back(column.name);
  }
  return names;
}
