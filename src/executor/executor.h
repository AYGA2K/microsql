#pragma once

#include "buffer_pool/buffer_pool.h"
#include "catalog/catalog.h"
#include "parser/parser.h"
#include "storage/row.h"
#include <string>
#include <vector>

enum class ExecError {
  TableSchemaNotFound,
  TableFileNotFound,
  ErrorGettingNumberOfPages,
  ErrorReadingPage,
  ErrorDeserializeRow,
  ErrorSerializeRow,
  ErrorGettingData,
  ErrorInsertingRow,
  ErrorDeletingRow,
  TypeMismatch,
  DivisionByZero,
  InvalidExpression,
  GettingRowFromInsertValues,
  ColumnCountMismatch,
  NotNullViolation,
  NotImplemented
};

struct Result {
  bool success = true;
  std::string message;
  std::vector<std::string> columns;
  std::vector<Row> rows;
};

struct ExecutionContext {
  Catalog catalog;
  BufferPool pool;
  std::unordered_map<std::string, TableFile *> openFiles;
};

Result execute(const ParseResult &parseResult, ExecutionContext &ctx);
