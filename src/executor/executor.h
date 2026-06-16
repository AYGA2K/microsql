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
  TypeMismatch,
  DivisionByZero,
  InvalidExpression,
  GettingRowFromInsertValues,
  NotImplemented
};

struct Result {
  std::vector<std::string> columns;
  std::vector<Row> rows;
};

struct ExecutionContext {
  Catalog catalog;
  BufferPool pool;
  std::unordered_map<std::string, TableFile *> openFiles;
};

std::expected<Result, ExecError> execute(const ParseResult &parseResult,
                                         ExecutionContext &ctx);
