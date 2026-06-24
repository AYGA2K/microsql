#include "executor.h"
#include "ast/statement.h"
#include "executor_helpers.h"
#include "storage/page.h"
#include "storage/row.h"
#include <cstddef>
#include <expected>
#include <print>

std::expected<Result, ExecError> execSelect(const ParseResult &parseResult,
                                            ExecutionContext &ctx) {
  TableSchema *tableSchema =
      ctx.catalog.findTable(parseResult.statement.tableName);
  if (!tableSchema) {
    std::println(stderr, "[Executor] table schema not found for table '{}'",
                 parseResult.statement.tableName);
    return std::unexpected(ExecError::TableSchemaNotFound);
  }

  // Resolve output columns: SELECT * expands to all schema columns
  Result result;
  if (parseResult.statement.selectColumns.empty()) {
    for (const ColumnDefinition &column : tableSchema->columns) {
      result.columns.push_back(column.name);
    }
  } else {
    for (int columnIndex : parseResult.statement.selectColumns) {
      result.columns.push_back(parseResult.expressions[columnIndex].columnName);
    }
  }

  TableFile *tableFile = ctx.openFiles.at(parseResult.statement.tableName);
  if (!tableFile) {
    std::println(stderr, "[Executor] table file not found for table '{}'",
                 parseResult.statement.tableName);
    return std::unexpected(ExecError::TableFileNotFound);
  }

  auto numPages = tableFile->numPages();
  if (!numPages) {
    std::println(
        stderr, "[Executor] failed to get number of pages for table '{}': {}",
        parseResult.statement.tableName, tableFileErrorStr(numPages.error()));
    return std::unexpected(ExecError::ErrorGettingNumberOfPages);
  }

  // Page 0 is the file header; data pages start at index 1
  int whereIndex = parseResult.statement.whereIndex;
  for (size_t i = 1; i < numPages.value(); i++) {
    auto page = tableFile->readPage(i);
    if (!page) {
      std::println(
          stderr, "[Executor] failed to read page {} of table '{}': {}", i,
          parseResult.statement.tableName, tableFileErrorStr(page.error()));
      return std::unexpected(ExecError::ErrorReadingPage);
    }

    auto row = deserializeRow(page.value()->data, tableSchema->columns);
    if (!row) {
      std::println(
          stderr,
          "[Executor] failed to deserialize row from page {} of table '{}'", i,
          parseResult.statement.tableName);
      return std::unexpected(ExecError::ErrorDeserializeRow);
    }

    // whereIndex == -1 means no WHERE clause; include every row
    if (whereIndex == -1) {
      result.rows.push_back(row.value());
    } else {
      auto match = evalWhere(parseResult.expressions[whereIndex], parseResult,
                             row.value(), result.columns);
      if (!match) {
        std::println(stderr,
                     "[Executor] failed to evaluate WHERE clause on page {} of "
                     "table '{}'",
                     i, parseResult.statement.tableName);
        return std::unexpected(match.error());
      }
      if (match.value()) {
        result.rows.push_back(row.value());
      }
    }
  }

  return result;
}

static std::expected<void, ExecError>
insertRowIntoPage(Page *page, const uint8_t *rowBytes, uint16_t rowSize,
                  TableFile *tableFile, const std::string &tableName) {
  auto ok = page->insertRow(rowBytes, rowSize);
  if (!ok) {
    std::println(stderr,
                 "[Executor] failed to insert row in page {} of table '{}': {}",
                 page->pageId, tableName, pageErrorStr(ok.error()));
    return std::unexpected(ExecError::ErrorInsertingRow);
  }
  auto written = tableFile->writePage(*page);
  if (!written) {
    std::println(stderr, "[Executor] failed to write page {} of table '{}': {}",
                 page->pageId, tableName, tableFileErrorStr(written.error()));
    return std::unexpected(ExecError::ErrorInsertingRow);
  }
  return {};
}

std::expected<Result, ExecError> execInsert(const ParseResult &parseResult,
                                            ExecutionContext &ctx) {
  TableSchema *tableSchema =
      ctx.catalog.findTable(parseResult.statement.tableName);
  if (!tableSchema) {
    std::println(stderr, "[Executor] table schema not found for table '{}'",
                 parseResult.statement.tableName);
    return std::unexpected(ExecError::TableSchemaNotFound);
  }

  TableFile *tableFile = ctx.openFiles.at(parseResult.statement.tableName);
  if (!tableFile) {
    std::println(stderr, "[Executor] table file not found for table '{}'",
                 parseResult.statement.tableName);
    return std::unexpected(ExecError::TableFileNotFound);
  }

  auto row = rowFromInsertValues(parseResult, tableSchema->columns);
  if (!row) {
    std::println(
        stderr,
        "[Executor] failed to get row from insert values for table '{}'",
        parseResult.statement.tableName);
    return std::unexpected(row.error());
  }

  if (parseResult.statement.insertColumnNames.empty() &&
      row.value().size() != tableSchema->columns.size()) {
    std::println(stderr,
                 "[Executor] column count mismatch on insert for table '{}': "
                 "expected {}, got {}",
                 parseResult.statement.tableName, tableSchema->columns.size(),
                 row.value().size());
    return std::unexpected(ExecError::ColumnCountMismatch);
  }

  auto rowBytes = serializeRow(row.value(), tableSchema->columns);
  if (!rowBytes) {
    std::println(
        stderr, "[Executor] failed to serialize row for table '{}': {}",
        parseResult.statement.tableName, rowErrorStr(rowBytes.error()));
    return std::unexpected(ExecError::ErrorSerializeRow);
  }

  auto numPages = tableFile->numPages();
  if (!numPages) {
    std::println(
        stderr, "[Executor] failed to get number of pages for table '{}': {}",
        parseResult.statement.tableName, tableFileErrorStr(numPages.error()));
    return std::unexpected(ExecError::ErrorGettingNumberOfPages);
  }

  uint16_t rowsize = rowSize(tableSchema->columns);
  size_t i = 1;
  while (i < numPages.value()) {
    auto page = tableFile->readPage(i);
    if (!page) {
      std::println(
          stderr, "[Executor] failed to read page {} of table '{}': {}", i,
          parseResult.statement.tableName, tableFileErrorStr(page.error()));
      return std::unexpected(ExecError::ErrorReadingPage);
    }
    if (page.value()->freeSpace() >= rowsize) {
      auto ok = insertRowIntoPage(page.value(), rowBytes.value().data(), rowsize,
                                  tableFile, parseResult.statement.tableName);
      if (!ok) {
        return std::unexpected(ok.error());
      }
      break;
    }
    i++;
  }
  // No existing page had enough free space => allocate a new one
  if (i == numPages.value()) {
    auto pageId = tableFile->allocatePage();
    if (!pageId) {
      std::println(
          stderr, "[Executor] failed to allocate page for table '{}': {}",
          parseResult.statement.tableName, tableFileErrorStr(pageId.error()));
      return std::unexpected(ExecError::ErrorInsertingRow);
    }
    auto page = tableFile->readPage(pageId.value());
    if (!page) {
      std::println(stderr,
                   "[Executor] failed to read page {} of table '{}': {}",
                   pageId.value(), parseResult.statement.tableName,
                   tableFileErrorStr(page.error()));
      return std::unexpected(ExecError::ErrorReadingPage);
    }
    auto ok = insertRowIntoPage(page.value(), rowBytes.value().data(), rowsize,
                                tableFile, parseResult.statement.tableName);
    if (!ok) {
      return std::unexpected(ok.error());
    }
  }
  return Result{};
}

std::expected<Result, ExecError> execute(const ParseResult &parseResult,
                                         ExecutionContext &ctx) {
  switch (parseResult.statement.kind) {
  case StatementKind::SELECT:
    return execSelect(parseResult, ctx);
  case StatementKind::INSERT:
    return execInsert(parseResult, ctx);
  case StatementKind::UPDATE:
  case StatementKind::DELETE:
  case StatementKind::CREATE_TABLE:
  case StatementKind::DROP_TABLE:
  case StatementKind::CREATE_INDEX:
  case StatementKind::BEGIN:
  case StatementKind::COMMIT:
  case StatementKind::ROLLBACK:
    break;
  }
  return std::unexpected(ExecError::NotImplemented);
}
