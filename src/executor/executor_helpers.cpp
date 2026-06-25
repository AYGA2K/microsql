#include "executor_helpers.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "executor/executor.h"
#include "parser/parser.h"
#include "storage/page.h"
#include "storage/row.h"
#include <cstddef>
#include <vector>

const char *tableFileErrorStr(TableFileError error) {
  switch (error) {
  case TableFileError::FileNotOpen:
    return "FileNotOpen";
  case TableFileError::FailedToOpenFile:
    return "FailedToOpenFile";
  case TableFileError::FailedToSeekPage:
    return "FailedToSeekPage";
  case TableFileError::FailedToReadPage:
    return "FailedToReadPage";
  case TableFileError::FailedToWritePage:
    return "FailedToWritePage";
  case TableFileError::FailedToGetFileSize:
    return "FailedToGetFileSize";
  }
  return "Unknown";
}

const char *pageErrorStr(PageError error) {
  switch (error) {
  case PageError::InvalidRowLength:
    return "InvalidRowLength";
  case PageError::PageFull:
    return "PageFull";
  case PageError::SlotOutOfBounds:
    return "SlotOutOfBounds";
  case PageError::SlotDeleted:
    return "SlotDeleted";
  }
  return "Unknown";
}

const char *rowErrorStr(RowError error) {
  switch (error) {
  case RowError::SchemaMismatch:
    return "SchemaMismatch";
  case RowError::DataTypeNotSupported:
    return "DataTypeNotSupported";
  }
  return "Unknown";
}

const char *execErrorStr(ExecError error) {
  switch (error) {
  // user-facing errors
  case ExecError::TableSchemaNotFound:
    return "table not found";
  case ExecError::ErrorGettingData:
    return "column not found";
  case ExecError::TypeMismatch:
    return "type mismatch";
  case ExecError::DivisionByZero:
    return "division by zero";
  case ExecError::InvalidExpression:
    return "invalid expression";
  case ExecError::ColumnCountMismatch:
    return "column count mismatch";
  case ExecError::NotNullViolation:
    return "NOT NULL constraint violated";
  case ExecError::NotImplemented:
    return "not implemented";
  // internal errors — details already logged to stderr
  case ExecError::TableFileNotFound:
  case ExecError::ErrorGettingNumberOfPages:
  case ExecError::ErrorReadingPage:
  case ExecError::ErrorDeserializeRow:
  case ExecError::ErrorSerializeRow:
  case ExecError::ErrorInsertingRow:
  case ExecError::ErrorDeletingRow:
  case ExecError::GettingRowFromInsertValues:
    return "internal error";
  }
  return "internal error";
}

size_t indexOf(const std::vector<std::string> &vec, const std::string &value) {
  auto it = std::find(vec.begin(), vec.end(), value);
  if (it == vec.end()) {
    return -1;
  }
  return it - vec.begin();
}

std::expected<Value, ExecError>
columnValue(const Row &row, const std::vector<std::string> &columns,
            const std::string &name) {
  try {
    return row.at(indexOf(columns, name));
  } catch (const std::out_of_range &) {
    return std::unexpected(ExecError::ErrorGettingData);
  }
}

std::expected<int, ExecError> compareValues(const Value &leftValue,
                                            const Value &rightValue) {
  if (leftValue.index() != rightValue.index()) {
    return std::unexpected(ExecError::TypeMismatch);
  }

  if (std::holds_alternative<int64_t>(leftValue)) {
    int64_t left = std::get<int64_t>(leftValue),
            right = std::get<int64_t>(rightValue);
    if (left != right) {
      return left < right ? -1 : 1;
    }
    return 0;
  }
  if (std::holds_alternative<double>(leftValue)) {
    double left = std::get<double>(leftValue),
           right = std::get<double>(rightValue);
    if (left != right) {
      return left < right ? -1 : 1;
    }
    return 0;
  }
  if (std::holds_alternative<std::string>(leftValue)) {
    return std::get<std::string>(leftValue).compare(
        std::get<std::string>(rightValue));
  }
  return std::unexpected(ExecError::TypeMismatch);
}

static std::expected<Value, ExecError>
applyArithmetic(const Value &left, const Value &right, BinaryOperator op) {
  if (left.index() != right.index()) {
    return std::unexpected(ExecError::TypeMismatch);
  }
  if (std::holds_alternative<int64_t>(left)) {
    int64_t l = std::get<int64_t>(left), r = std::get<int64_t>(right);
    switch (op) {
    case BinaryOperator::ADD:
      return l + r;
    case BinaryOperator::SUBTRACT:
      return l - r;
    case BinaryOperator::MULTIPLY:
      return l * r;
    case BinaryOperator::DIVIDE:
      if (r == 0) {
        return std::unexpected(ExecError::DivisionByZero);
      }
      return l / r;
    default:
      break;
    }
  }
  if (std::holds_alternative<double>(left)) {
    double l = std::get<double>(left), r = std::get<double>(right);
    switch (op) {
    case BinaryOperator::ADD:
      return l + r;
    case BinaryOperator::SUBTRACT:
      return l - r;
    case BinaryOperator::MULTIPLY:
      return l * r;
    case BinaryOperator::DIVIDE:
      return l / r;
    default:
      break;
    }
  }
  if (std::holds_alternative<std::string>(left) && op == BinaryOperator::ADD) {
    return std::get<std::string>(left) + std::get<std::string>(right);
  }
  return std::unexpected(ExecError::TypeMismatch);
}

static std::expected<Value, ExecError> flipSignValue(const Value &value) {
  if (std::holds_alternative<int64_t>(value)) {
    return -std::get<int64_t>(value);
  }
  if (std::holds_alternative<double>(value)) {
    return -std::get<double>(value);
  }
  return std::unexpected(ExecError::TypeMismatch);
}

static std::expected<Value, ExecError> flipBoolValue(const Value &value) {
  if (std::holds_alternative<bool>(value)) {
    return !std::get<bool>(value);
  }
  return std::unexpected(ExecError::TypeMismatch);
}

std::expected<Value, ExecError>
evalExpr(const Expression &expr, const ParseResult &parseResult, const Row &row,
         const std::vector<std::string> &columns) {
  switch (expr.kind) {
  case ExpressionKind::LITERAL_BOOL:
    return expr.boolValue;
  case ExpressionKind::LITERAL_INT:
    return expr.intValue;
  case ExpressionKind::LITERAL_FLOAT:
    return expr.floatValue;
  case ExpressionKind::LITERAL_TEXT:
    return expr.textValue;
  case ExpressionKind::LITERAL_NULL:
    return nullptr;
  case ExpressionKind::COLUMN_REF:
    if (expr.tablePrefix.empty()) {
      return columnValue(row, columns, expr.columnName);
    }
    // TODO: handle cases like users.name
    return std::unexpected(ExecError::NotImplemented);
  case ExpressionKind::BINARY: {
    auto left = evalExpr(parseResult.expressions[expr.leftIndex], parseResult,
                         row, columns);
    if (!left) {
      return std::unexpected(left.error());
    }
    auto right = evalExpr(parseResult.expressions[expr.rightIndex], parseResult,
                          row, columns);
    if (!right) {
      return std::unexpected(right.error());
    }
    switch (expr.binaryOperator) {
    case BinaryOperator::ADD:
    case BinaryOperator::SUBTRACT:
    case BinaryOperator::MULTIPLY:
    case BinaryOperator::DIVIDE:
      return applyArithmetic(*left, *right, expr.binaryOperator);
    default:
      return std::unexpected(ExecError::InvalidExpression);
    }
  }
  case ExpressionKind::UNARY: {
    auto unaryExpr = evalExpr(parseResult.expressions[expr.operandIndex],
                              parseResult, row, columns);
    if (!unaryExpr) {
      return std::unexpected(unaryExpr.error());
    }
    switch (expr.unaryOperator) {
    case UnaryOperator::NEGATE:
      return flipSignValue(*unaryExpr);
    case UnaryOperator::NOT:
      return flipBoolValue(*unaryExpr);
    }
  }
  case ExpressionKind::STAR:
    return std::unexpected(ExecError::InvalidExpression);
  }
}

std::expected<Row, ExecError>
rowFromInsertValues(const ParseResult &parseResult,
                    std::vector<ColumnDefinition> columns) {
  Row row;
  if (parseResult.statement.insertColumnNames.empty()) {
    for (int index : parseResult.statement.insertValues) {
      auto value =
          evalExpr(parseResult.expressions[index], parseResult, {}, {});
      if (!value) {
        return std::unexpected(value.error());
      }
      row.push_back(std::move(*value));
    }
    return row;
  }
  for (const ColumnDefinition &col : columns) {
    const auto &columnNames = parseResult.statement.insertColumnNames;
    auto it = std::find(columnNames.begin(), columnNames.end(), col.name);
    if (it != columnNames.end()) {
      size_t pos = it - columnNames.begin();
      int exprIndex = parseResult.statement.insertValues[pos];
      auto value =
          evalExpr(parseResult.expressions[exprIndex], parseResult, {}, {});
      if (!value) {
        return std::unexpected(value.error());
      }
      row.push_back(std::move(*value));
    } else {
      if (col.notNull) {
        return std::unexpected(ExecError::NotNullViolation);
      }
      row.push_back(nullptr);
    }
  }

  return row;
}

std::expected<bool, ExecError>
evalWhere(const Expression &expr, const ParseResult &parseResult,
          const Row &row, const std::vector<std::string> &columns) {
  if (expr.kind != ExpressionKind::BINARY) {
    return true;
  }
  const Expression &left = parseResult.expressions[expr.leftIndex];
  const Expression &right = parseResult.expressions[expr.rightIndex];

  if (expr.binaryOperator == BinaryOperator::AND) {
    auto leftResult = evalWhere(left, parseResult, row, columns);
    if (!leftResult) {
      return std::unexpected(leftResult.error());
    }
    auto rightResult = evalWhere(right, parseResult, row, columns);
    if (!rightResult) {
      return std::unexpected(rightResult.error());
    }
    return *leftResult && *rightResult;
  }
  if (expr.binaryOperator == BinaryOperator::OR) {
    auto leftResult = evalWhere(left, parseResult, row, columns);
    if (!leftResult) {
      return std::unexpected(leftResult.error());
    }
    auto rightResult = evalWhere(right, parseResult, row, columns);
    if (!rightResult) {
      return std::unexpected(rightResult.error());
    }
    return *leftResult || *rightResult;
  }

  auto leftValue = evalExpr(left, parseResult, row, columns);
  if (!leftValue) {
    return std::unexpected(leftValue.error());
  }
  auto rightValue = evalExpr(right, parseResult, row, columns);
  if (!rightValue) {
    return std::unexpected(rightValue.error());
  }

  switch (expr.binaryOperator) {
  case BinaryOperator::EQUAL:
    return *leftValue == *rightValue;
  case BinaryOperator::NOT_EQUAL:
    return *leftValue != *rightValue;
  case BinaryOperator::LESS_THAN:
  case BinaryOperator::LESS_THAN_OR_EQUAL:
  case BinaryOperator::GREATER_THAN:
  case BinaryOperator::GREATER_THAN_OR_EQUAL: {
    auto comparison = compareValues(*leftValue, *rightValue);
    if (!comparison) {
      return std::unexpected(comparison.error());
    }
    if (expr.binaryOperator == BinaryOperator::LESS_THAN) {
      return *comparison < 0;
    }
    if (expr.binaryOperator == BinaryOperator::LESS_THAN_OR_EQUAL) {
      return *comparison <= 0;
    }
    if (expr.binaryOperator == BinaryOperator::GREATER_THAN) {
      return *comparison > 0;
    }
    return *comparison >= 0;
  }
  case BinaryOperator::IS: {
    bool leftIsNull = std::holds_alternative<std::nullptr_t>(*leftValue);
    bool rightIsNull = std::holds_alternative<std::nullptr_t>(*rightValue);
    if (leftIsNull && rightIsNull) {
      return true;
    }
    if (leftIsNull || rightIsNull) {
      return false;
    }
    return *leftValue == *rightValue;
  }
  case BinaryOperator::IS_NOT: {
    bool leftIsNull = std::holds_alternative<std::nullptr_t>(*leftValue);
    bool rightIsNull = std::holds_alternative<std::nullptr_t>(*rightValue);
    if (leftIsNull && rightIsNull) {
      return false;
    }
    if (leftIsNull || rightIsNull) {
      return true;
    }
    return *leftValue != *rightValue;
  }
  default:
    break;
  }
  return std::unexpected(ExecError::NotImplemented);
}
