#pragma once

#include "ast/expression.h"
#include "ast/statement.h"
#include "executor.h"
#include "parser/parser.h"
#include "storage/page.h"
#include "storage/row.h"
#include <expected>
#include <string>
#include <vector>

const char *tableFileErrorStr(TableFileError error);
const char *pageErrorStr(PageError error);
const char *rowErrorStr(RowError error);
const char *execErrorStr(ExecError error);
const char *catalogErrorStr(CatalogError error);

int indexOf(const std::vector<std::string> &vec, const std::string &value);

std::expected<Value, ExecError>
columnValue(const Row &row, const std::vector<std::string> &columns,
            const std::string &name);

std::expected<int, ExecError> compareValues(const Value &leftValue,
                                            const Value &rightValue);

std::expected<Value, ExecError>
evalExpr(const Expression &expr, const ParseResult &parseResult, const Row &row,
         const std::vector<std::string> &columns);

std::expected<bool, ExecError>
evalWhere(const Expression &expr, const ParseResult &parseResult,
          const Row &row, const std::vector<std::string> &columns);

std::expected<Row, ExecError>
rowFromInsertValues(const ParseResult &parseResult,
                    std::vector<ColumnDefinition> columns);

std::expected<Row, ExecError>
rowFromUpdate(const Row &row,
              const std::vector<std::pair<std::string, int>> &assignments,
              const ParseResult &parseResult,
              const std::vector<std::string> &columnNames);
