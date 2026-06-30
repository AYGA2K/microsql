#pragma once
#include "../ast/statement.h"
#include <cstdint>
#include <expected>
#include <string>
#include <variant>
#include <vector>

enum class RowError { SchemaMismatch, DataTypeNotSupported };

// nullptr_t represents SQL NULL
using Value = std::variant<int64_t, double, std::string, bool, std::nullptr_t>;
using Row = std::vector<Value>;
using Schema = std::vector<ColumnDefinition>;

std::expected<std::vector<uint8_t>, RowError> serializeRow(const Row &row,
                                                           const Schema &schema);

std::expected<Row, RowError> deserializeRow(const uint8_t *data,
                                            const Schema &schema);

