#pragma once

#include "catalog/catalog.h"
#include "parser/parser.h"
#include "storage/page.h"
#include "storage/row.h"
#include <string>
#include <unordered_map>
#include <vector>

enum class ExecError {
  TableSchemaNotFound,
  ErrorGettingData,
  TypeMismatch,
  DivisionByZero,
  InvalidExpression,
  ColumnCountMismatch,
  NotNullViolation,
  NotImplemented,
  DuplicateTable,
  InternalError,
};

struct Result {
  bool success = true;
  std::string message;
  std::vector<std::string> columns;
  std::vector<Row> rows;
};

struct ExecutionContext {
  Catalog catalog;
  std::unordered_map<std::string, TableFile *> openFiles; // lazily opened on first access
};

Result execute(const ParseResult &parseResult, ExecutionContext &ctx);
