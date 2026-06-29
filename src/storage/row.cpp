#include "row.h"
#include "ast/statement.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <string>

std::expected<std::vector<uint8_t>, RowError> serializeRow(const Row &row,
                                                           const Schema &schema) {
  if (row.size() != schema.size()) {
    return std::unexpected(RowError::SchemaMismatch);
  }
  std::vector<uint8_t> output(rowSize(schema));
  size_t offset = 0;
  for (size_t i = 0; i < row.size(); ++i) {
    const Value &value = row[i];
    switch (schema[i].type) {
    case DataType::BOOLEAN: {
      bool v = std::get<bool>(value);
      std::memcpy(output.data() + offset, &v, schema[i].size());
      offset += schema[i].size();
      break;
    }
    case DataType::INTEGER: {
      int64_t v = std::get<int64_t>(value);
      std::memcpy(output.data() + offset, &v, schema[i].size());
      offset += schema[i].size();
      break;
    }
    case DataType::FLOAT: {
      double v = std::get<double>(value);
      std::memcpy(output.data() + offset, &v, schema[i].size());
      offset += schema[i].size();
      break;
    }
    case DataType::TEXT: {
      const std::string &v = std::get<std::string>(value);
      // Copy only as many bytes as the string holds
      size_t len = std::min(v.size(), static_cast<size_t>(schema[i].size()));
      std::memcpy(output.data() + offset, v.data(), len);
      offset += schema[i].size();
      break;
    }
    default:
      return std::unexpected(RowError::DataTypeNotSupported);
    }
  }
  return output;
}

std::expected<Row, RowError> deserializeRow(const uint8_t *data,
                                            const Schema &schema) {
  Row row;
  size_t offset = 0;
  for (const ColumnDefinition &colDef : schema) {
    switch (colDef.type) {
    case DataType::BOOLEAN: {
      bool val;
      std::memcpy(&val, data + offset, colDef.size());
      row.push_back(val);
      offset += colDef.size();
      break;
    }
    case DataType::INTEGER: {
      int64_t val;
      std::memcpy(&val, data + offset, colDef.size());
      row.push_back(val);
      offset += colDef.size();
      break;
    }
    case DataType::FLOAT: {
      double val;
      std::memcpy(&val, data + offset, colDef.size());
      row.push_back(val);
      offset += colDef.size();
      break;
    }
    case DataType::TEXT: {
      std::string val(reinterpret_cast<const char *>(data + offset),
                      colDef.size());
      // Strip the null-padding that fills the rest of the fixed-width field
      size_t end = val.find('\0');
      if (end != std::string::npos) {
        val.resize(end);
      }
      row.push_back(std::move(val));
      offset += colDef.size();
      break;
    }
    default:
      return std::unexpected(RowError::DataTypeNotSupported);
    }
  }

  return row;
}

uint16_t rowSize(const Schema &schema) {
  uint16_t size = 0;
  for (const ColumnDefinition &col : schema) {
    size += col.size();
  }
  return size;
}
