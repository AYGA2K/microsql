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
  size_t totalSize = 0;
  for (size_t i = 0; i < row.size(); ++i) {
    if (schema[i].type == DataType::TEXT) {
      const std::string &str = std::get<std::string>(row[i]);
      if (str.size() > static_cast<size_t>(schema[i].textLength)) {
        return std::unexpected(RowError::TextTooLong);
      }
      totalSize += 2 + str.size();
    } else {
      totalSize += schema[i].size();
    }
  }
  std::vector<uint8_t> output(totalSize);
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
      const std::string &str = std::get<std::string>(value);
      uint16_t len = static_cast<uint16_t>(str.size());
      std::memcpy(output.data() + offset, &len, 2);
      std::memcpy(output.data() + offset + 2, str.data(), len);
      offset += 2 + len;
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
      uint16_t len;
      std::memcpy(&len, data + offset, 2);
      std::string str(reinterpret_cast<const char *>(data + offset + 2), len);
      row.push_back(std::move(str));
      offset += 2 + len;
      break;
    }
    default:
      return std::unexpected(RowError::DataTypeNotSupported);
    }
  }

  return row;
}

