
#include "catalog/catalog.h"
#include "ast/statement.h"
#include "catalog/tableschema.h"
#include <expected>
#include <print>
#include <fstream>
#include <sstream>
#include <string>

static const char *dataTypeName(DataType t) {
  switch (t) {
  case DataType::INTEGER: return "INTEGER";
  case DataType::FLOAT:   return "FLOAT";
  case DataType::TEXT:    return "TEXT";
  case DataType::BOOLEAN: return "BOOLEAN";
  }
  return "";
}
std::vector<std::string> split(const std::string &s) {
  std::stringstream ss(s);
  std::vector<std::string> result;
  std::string word;

  while (ss >> word) {
    result.push_back(word);
  }

  return result;
}

std::expected<void, CatalogError> Catalog::load(const std::string &directory) {
  std::string path = directory + "/catalog.txt";
  std::ifstream file(path);
  if (!file.is_open()) {
    std::println(stderr, "[Catalog] failed to open '{}'", path);
    return std::unexpected(CatalogError::FileNotOpen);
  }
  std::string line;
  int lineNum = 0;
  while (std::getline(file, line)) {
    lineNum++;
    if (line.starts_with("TABLE")) {
      TableSchema tableSchema;
      auto words = split(line);
      if (words.size() != 2) {
        std::println(stderr,
                     "[Catalog] line {}: expected 'TABLE <name>', got '{}' ({} token{})",
                     lineNum, line, words.size(), words.size() == 1 ? "" : "s");
        return std::unexpected(CatalogError::UnvalidTableLineFormat);
      }
      tableSchema.tableName = words[1];
      while (std::getline(file, line) && line != "END") {
        lineNum++;
        if (line.starts_with("COLUMN")) {
          words = split(line);
          if (words.size() != 6) {
            std::println(stderr,
                         "[Catalog] line {} in table '{}': expected 'COLUMN <name> <type> "
                         "<textLength> <notNull> <primaryKey>', got '{}' ({} token{})",
                         lineNum, tableSchema.tableName, line, words.size(),
                         words.size() == 1 ? "" : "s");
            return std::unexpected(CatalogError::UnvalidColumnLineFormat);
          }
          ColumnDefinition column;
          column.name = words[1];
          if (words[2] == "INTEGER") {
            column.type = DataType::INTEGER;
          } else if (words[2] == "FLOAT") {
            column.type = DataType::FLOAT;
          } else if (words[2] == "TEXT") {
            column.type = DataType::TEXT;
          } else if (words[2] == "BOOLEAN") {
            column.type = DataType::BOOLEAN;
          }
          column.textLength = std::stoi(words[3]);
          column.notNull = words[4] == "true";
          column.primaryKey = words[5] == "true";
          tableSchema.columns.push_back(column);
        } else if (line.starts_with("FILE")) {
          words = split(line);
          if (words.size() == 2) {
            tableSchema.filePath = words[1];
          }
        }
      }
      tables.push_back(tableSchema);
    }
  }

  file.close();
  return {};
}

TableSchema *Catalog::findTable(const std::string &name) {
  for (auto &t : tables) {
    if (t.tableName == name)
      return &t;
  }
  return nullptr;
}

void Catalog::addTable(const TableSchema &schema) {
  tables.push_back(schema);
}

void Catalog::dropTable(const std::string &name) {
  std::erase_if(tables, [&](const TableSchema &t) { return t.tableName == name; });
}

void Catalog::save(const std::string &directory) {
  std::ofstream file(directory + "/catalog.txt");
  for (const auto &table : tables) {
    file << "TABLE " << table.tableName << "\n";
    for (const auto &col : table.columns) {
      file << "COLUMN " << col.name
           << " " << dataTypeName(col.type)
           << " " << col.textLength
           << " " << (col.notNull    ? "true" : "false")
           << " " << (col.primaryKey ? "true" : "false")
           << "\n";
    }
    file << "FILE " << table.filePath << "\n";
    file << "END\n";
  }
}
