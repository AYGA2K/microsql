#include "executor.h"
#include "ast/statement.h"
#include "executor_helpers.h"
#include "storage/page.h"
#include "storage/row.h"
#include <cstddef>
#include <expected>
#include <format>
#include <print>

struct ResolvedTable {
  TableSchema *schema;
  TableFile *file;
};

static std::expected<ResolvedTable, ExecError>
resolveTable(const std::string &tableName, ExecutionContext &ctx) {
  TableSchema *schema = ctx.catalog.findTable(tableName);
  if (!schema) {
    std::println(stderr, "[Executor] table schema not found for table '{}'",
                 tableName);
    return std::unexpected(ExecError::TableSchemaNotFound);
  }
  TableFile *file = ctx.openFiles.at(tableName);
  if (!file) {
    std::println(stderr, "[Executor] table file not found for table '{}'",
                 tableName);
    return std::unexpected(ExecError::TableFileNotFound);
  }
  return ResolvedTable{schema, file};
}

static std::expected<size_t, ExecError>
getNumPages(TableFile *file, const std::string &tableName) {
  auto numPages = file->numPages();
  if (!numPages) {
    std::println(stderr,
                 "[Executor] failed to get number of pages for table '{}': {}",
                 tableName, tableFileErrorStr(numPages.error()));
    return std::unexpected(ExecError::ErrorGettingNumberOfPages);
  }
  return numPages.value();
}

static std::expected<Page *, ExecError>
readPage(TableFile *file, size_t pageIndex, const std::string &tableName) {
  auto page = file->readPage(pageIndex);
  if (!page) {
    std::println(stderr, "[Executor] failed to read page {} of table '{}': {}",
                 pageIndex, tableName, tableFileErrorStr(page.error()));
    return std::unexpected(ExecError::ErrorReadingPage);
  }
  return page.value();
}

static std::expected<Row, ExecError>
readSlotRow(Page *page, uint16_t slot, size_t pageIndex,
            const std::string &tableName,
            const std::vector<ColumnDefinition> &columns) {
  uint16_t rowLen;
  auto rowData = page->readRow(slot, &rowLen);
  if (!rowData) {
    std::println(stderr,
                 "[Executor] failed to read slot {} on page {} of table '{}'",
                 slot, pageIndex, tableName);
    return std::unexpected(ExecError::ErrorReadingPage);
  }
  auto row = deserializeRow(rowData.value(), columns);
  if (!row) {
    std::println(stderr,
                 "[Executor] failed to deserialize row from slot {} on page "
                 "{} of table '{}'",
                 slot, pageIndex, tableName);
    return std::unexpected(ExecError::ErrorDeserializeRow);
  }
  return row.value();
}

std::expected<Result, ExecError> execSelect(const ParseResult &parseResult,
                                            ExecutionContext &ctx) {
  auto resolved = resolveTable(parseResult.statement.tableName, ctx);
  if (!resolved) {
    return std::unexpected(resolved.error());
  }
  auto [schema, file] = resolved.value();

  Result result;
  if (parseResult.statement.selectColumns.empty()) {
    result.columns = schema->columnNames();
  } else {
    for (int columnIndex : parseResult.statement.selectColumns) {
      result.columns.push_back(parseResult.expressions[columnIndex].columnName);
    }
  }

  auto numPages = getNumPages(file, parseResult.statement.tableName);
  if (!numPages) {
    return std::unexpected(numPages.error());
  }

  int whereIndex = parseResult.statement.whereIndex;
  for (size_t pageIndex = 1; pageIndex < numPages.value(); pageIndex++) {
    auto page = readPage(file, pageIndex, parseResult.statement.tableName);
    if (!page) {
      return std::unexpected(page.error());
    }

    uint16_t numSlots = page.value()->numSlots();
    for (uint16_t slot = 0; slot < numSlots; slot++) {
      if (page.value()->slotDeleted(slot)) {
        continue;
      }
      auto row = readSlotRow(page.value(), slot, pageIndex,
                             parseResult.statement.tableName, schema->columns);
      if (!row) {
        return std::unexpected(row.error());
      }
      if (whereIndex == -1) {
        result.rows.push_back(row.value());
      } else {
        auto match = evalWhere(parseResult.expressions[whereIndex], parseResult,
                               row.value(), result.columns);
        if (!match) {
          std::println(stderr,
                       "[Executor] failed to evaluate WHERE clause on slot {} "
                       "of page {} of table '{}'",
                       slot, pageIndex, parseResult.statement.tableName);
          return std::unexpected(match.error());
        }
        if (match.value()) {
          result.rows.push_back(row.value());
        }
      }
    }
  }

  return result;
}

static std::expected<void, ExecError>
insertRowIntoPage(Page *page, const uint8_t *rowBytes, uint16_t rowSizeBytes,
                  TableFile *tableFile, const std::string &tableName) {
  auto ok = page->insertRow(rowBytes, rowSizeBytes);
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

static std::expected<void, ExecError> execInsert(const ParseResult &parseResult,
                                                 ExecutionContext &ctx) {
  auto resolved = resolveTable(parseResult.statement.tableName, ctx);
  if (!resolved) {
    return std::unexpected(resolved.error());
  }
  auto [schema, file] = resolved.value();

  auto row = rowFromInsertValues(parseResult, schema->columns);
  if (!row) {
    std::println(
        stderr,
        "[Executor] failed to get row from insert values for table '{}'",
        parseResult.statement.tableName);
    return std::unexpected(row.error());
  }

  if (parseResult.statement.insertColumnNames.empty() &&
      row.value().size() != schema->columns.size()) {
    std::println(stderr,
                 "[Executor] column count mismatch on insert for table '{}': "
                 "expected {}, got {}",
                 parseResult.statement.tableName, schema->columns.size(),
                 row.value().size());
    return std::unexpected(ExecError::ColumnCountMismatch);
  }

  auto rowBytes = serializeRow(row.value(), schema->columns);
  if (!rowBytes) {
    std::println(
        stderr, "[Executor] failed to serialize row for table '{}': {}",
        parseResult.statement.tableName, rowErrorStr(rowBytes.error()));
    return std::unexpected(ExecError::ErrorSerializeRow);
  }

  auto numPages = getNumPages(file, parseResult.statement.tableName);
  if (!numPages) {
    return std::unexpected(numPages.error());
  }

  uint16_t rowSizeBytes = rowSize(schema->columns);
  size_t pageIndex = 1;
  while (pageIndex < numPages.value()) {
    auto page = readPage(file, pageIndex, parseResult.statement.tableName);
    if (!page) {
      return std::unexpected(page.error());
    }
    if (page.value()->freeSpace() >= rowSizeBytes) {
      auto ok =
          insertRowIntoPage(page.value(), rowBytes.value().data(), rowSizeBytes,
                            file, parseResult.statement.tableName);
      if (!ok) {
        return std::unexpected(ok.error());
      }
      break;
    }
    pageIndex++;
  }
  if (pageIndex == numPages.value()) {
    auto newPageId = file->allocatePage();
    if (!newPageId) {
      std::println(stderr,
                   "[Executor] failed to allocate page for table '{}': {}",
                   parseResult.statement.tableName,
                   tableFileErrorStr(newPageId.error()));
      return std::unexpected(ExecError::ErrorInsertingRow);
    }
    auto page =
        readPage(file, newPageId.value(), parseResult.statement.tableName);
    if (!page) {
      return std::unexpected(page.error());
    }
    auto ok =
        insertRowIntoPage(page.value(), rowBytes.value().data(), rowSizeBytes,
                          file, parseResult.statement.tableName);
    if (!ok) {
      return std::unexpected(ok.error());
    }
  }
  return {};
}

static std::expected<int, ExecError> execDelete(const ParseResult &parseResult,
                                                ExecutionContext &ctx) {
  auto resolved = resolveTable(parseResult.statement.tableName, ctx);
  if (!resolved) {
    return std::unexpected(resolved.error());
  }
  auto [schema, file] = resolved.value();

  auto numPages = getNumPages(file, parseResult.statement.tableName);
  if (!numPages) {
    return std::unexpected(numPages.error());
  }

  int deletedCount = 0;
  int whereIndex = parseResult.statement.whereIndex;
  size_t pageIndex = 1;
  while (pageIndex < numPages.value()) {
    auto page = readPage(file, pageIndex, parseResult.statement.tableName);
    if (!page) {
      return std::unexpected(page.error());
    }

    uint16_t numSlots = page.value()->numSlots();
    for (uint16_t slot = 0; slot < numSlots; slot++) {
      if (page.value()->slotDeleted(slot)) {
        continue;
      }
      auto row = readSlotRow(page.value(), slot, pageIndex,
                             parseResult.statement.tableName, schema->columns);
      if (!row) {
        return std::unexpected(row.error());
      }
      bool shouldDelete = false;
      if (whereIndex == -1) {
        shouldDelete = true;
      } else {
        auto match = evalWhere(parseResult.expressions[whereIndex], parseResult,
                               row.value(), schema->columnNames());
        if (!match) {
          std::println(stderr,
                       "[Executor] failed to evaluate WHERE clause on slot {} "
                       "of page {} of table '{}'",
                       slot, pageIndex, parseResult.statement.tableName);
          return std::unexpected(match.error());
        }
        shouldDelete = match.value();
      }
      if (shouldDelete) {
        auto ok = page.value()->deleteRow(slot);
        if (!ok) {
          std::println(stderr,
                       "[Executor] failed to delete slot {} on page {} of "
                       "table '{}'",
                       slot, pageIndex, parseResult.statement.tableName);
          return std::unexpected(ExecError::ErrorDeletingRow);
        }
        deletedCount++;
      }
    }
    auto written = file->writePage(*page.value());
    if (!written) {
      std::println(stderr,
                   "[Executor] failed to write page {} of table '{}': {}",
                   pageIndex, parseResult.statement.tableName,
                   tableFileErrorStr(written.error()));
      return std::unexpected(ExecError::ErrorInsertingRow);
    }
    pageIndex++;
  }

  return deletedCount;
}

static std::expected<int, ExecError> execUpdate(const ParseResult &parseResult,
                                                ExecutionContext &ctx) {
  auto resolved = resolveTable(parseResult.statement.tableName, ctx);
  if (!resolved) {
    return std::unexpected(resolved.error());
  }
  auto [schema, file] = resolved.value();

  auto numPages = getNumPages(file, parseResult.statement.tableName);
  if (!numPages) {
    return std::unexpected(numPages.error());
  }

  int updatedCount = 0;
  int whereIndex = parseResult.statement.whereIndex;
  size_t pageIndex = 1;
  while (pageIndex < numPages.value()) {
    auto page = readPage(file, pageIndex, parseResult.statement.tableName);
    if (!page) {
      return std::unexpected(page.error());
    }

    uint16_t numSlots = page.value()->numSlots();
    for (uint16_t slot = 0; slot < numSlots; slot++) {
      if (page.value()->slotDeleted(slot)) {
        continue;
      }
      auto row = readSlotRow(page.value(), slot, pageIndex,
                             parseResult.statement.tableName, schema->columns);
      if (!row) {
        return std::unexpected(row.error());
      }
      bool shouldUpdate = whereIndex == -1;
      if (!shouldUpdate) {
        auto match = evalWhere(parseResult.expressions[whereIndex], parseResult,
                               row.value(), schema->columnNames());
        if (!match) {
          std::println(stderr,
                       "[Executor] failed to evaluate WHERE clause on slot {} "
                       "of page {} of table '{}'",
                       slot, pageIndex, parseResult.statement.tableName);
          return std::unexpected(match.error());
        }
        shouldUpdate = match.value();
      }
      if (shouldUpdate) {
        auto updated =
            rowFromUpdate(row.value(), parseResult.statement.assignments,
                          parseResult, schema->columnNames());
        if (!updated) {
          return std::unexpected(updated.error());
        }
        auto rowBytes = serializeRow(updated.value(), schema->columns);
        if (!rowBytes) {
          std::println(stderr,
                       "[Executor] failed to serialize updated row for table '{}'",
                       parseResult.statement.tableName);
          return std::unexpected(ExecError::ErrorSerializeRow);
        }
        uint16_t rowSizeBytes = rowSize(schema->columns);
        auto ok = page.value()->updateRow(slot, rowBytes.value().data(), rowSizeBytes);
        if (!ok) {
          std::println(stderr,
                       "[Executor] failed to update slot {} on page {} of "
                       "table '{}'",
                       slot, pageIndex, parseResult.statement.tableName);
          return std::unexpected(ExecError::ErrorUpdatingRow);
        }
        updatedCount++;
      }
    }
    auto written = file->writePage(*page.value());
    if (!written) {
      std::println(stderr,
                   "[Executor] failed to write page {} of table '{}': {}",
                   pageIndex, parseResult.statement.tableName,
                   tableFileErrorStr(written.error()));
      return std::unexpected(ExecError::ErrorUpdatingRow);
    }
    pageIndex++;
  }

  return updatedCount;
}

static Result makeError(ExecError err) {
  return Result{.success = false, .message = execErrorStr(err), .columns = {}, .rows = {}};
}

Result execute(const ParseResult &parseResult, ExecutionContext &ctx) {
  switch (parseResult.statement.kind) {
  case StatementKind::SELECT: {
    auto result = execSelect(parseResult, ctx);
    if (!result) {
      return makeError(result.error());
    }
    int rowCount = result.value().rows.size();
    result.value().message =
        std::format("{} row{}", rowCount, rowCount == 1 ? "" : "s");
    return result.value();
  }
  case StatementKind::INSERT: {
    auto ok = execInsert(parseResult, ctx);
    if (!ok) {
      return makeError(ok.error());
    }
    return Result{.success = true, .message = "1 row inserted", .columns = {}, .rows = {}};
  }
  case StatementKind::DELETE: {
    auto count = execDelete(parseResult, ctx);
    if (!count) {
      return makeError(count.error());
    }
    int n = count.value();
    return Result{.success = true,
                  .message = std::format("{} row{} deleted", n, n == 1 ? "" : "s"),
                  .columns = {},
                  .rows = {}};
  }
  case StatementKind::UPDATE: {
    auto count = execUpdate(parseResult, ctx);
    if (!count) {
      return makeError(count.error());
    }
    int n = count.value();
    return Result{.success = true,
                  .message = std::format("{} row{} updated", n, n == 1 ? "" : "s"),
                  .columns = {},
                  .rows = {}};
  }
  case StatementKind::CREATE_TABLE:
  case StatementKind::DROP_TABLE:
  case StatementKind::CREATE_INDEX:
  case StatementKind::BEGIN:
  case StatementKind::COMMIT:
  case StatementKind::ROLLBACK:
    break;
  }
  return makeError(ExecError::NotImplemented);
}
