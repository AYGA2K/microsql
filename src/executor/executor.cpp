#include "executor.h"
#include "ast/statement.h"
#include "catalog/catalog.h"
#include "catalog/tableschema.h"
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
  auto it = ctx.openFiles.find(tableName);
  if (it == ctx.openFiles.end()) {
    TableFile *tableFile = new TableFile{};
    auto opened = tableFile->open(schema->filePath);
    if (!opened) {
      std::println(stderr, "[Executor] failed to open file for table '{}': {}",
                   tableName, tableFileErrorStr(opened.error()));
      delete tableFile;
      return std::unexpected(ExecError::InternalError);
    }
    ctx.openFiles[tableName] = tableFile;
    return ResolvedTable{schema, tableFile};
  }
  return ResolvedTable{schema, it->second};
}

static std::expected<size_t, ExecError>
getNumPages(TableFile *file, const std::string &tableName) {
  auto numPages = file->numPages();
  if (!numPages) {
    std::println(stderr,
                 "[Executor] failed to get number of pages for table '{}': {}",
                 tableName, tableFileErrorStr(numPages.error()));
    return std::unexpected(ExecError::InternalError);
  }
  return numPages.value();
}

static std::expected<Page *, ExecError>
readPage(TableFile *file, size_t pageIndex, const std::string &tableName) {
  auto page = file->readPage(pageIndex);
  if (!page) {
    std::println(stderr, "[Executor] failed to read page {} of table '{}': {}",
                 pageIndex, tableName, tableFileErrorStr(page.error()));
    return std::unexpected(ExecError::InternalError);
  }
  return page.value();
}

static std::expected<void, ExecError>
writePage(TableFile *file, Page *page, size_t pageIndex,
          const std::string &tableName) {
  auto written = file->writePage(*page);
  if (!written) {
    std::println(stderr, "[Executor] failed to write page {} of table '{}': {}",
                 pageIndex, tableName, tableFileErrorStr(written.error()));
    return std::unexpected(ExecError::InternalError);
  }
  return {};
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
    return std::unexpected(ExecError::InternalError);
  }
  auto row = deserializeRow(rowData.value(), columns);
  if (!row) {
    std::println(stderr,
                 "[Executor] failed to deserialize row from slot {} on page "
                 "{} of table '{}'",
                 slot, pageIndex, tableName);
    return std::unexpected(ExecError::InternalError);
  }
  return row.value();
}

static int primaryKeyIndex(const std::vector<ColumnDefinition> &columns) {
  for (size_t i = 0; i < columns.size(); i++) {
    if (columns[i].primaryKey) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// Returns true if some row already holds value in the primary key column.
static std::expected<bool, ExecError>
primaryKeyExists(TableFile *file, const std::string &tableName,
                 const std::vector<ColumnDefinition> &columns, int pkIndex,
                 const Value &value, Page *currentPage, size_t currentPageIndex,
                 int skipSlot) {
  auto numPages = getNumPages(file, tableName);
  if (!numPages) {
    return std::unexpected(numPages.error());
  }

  // Page 0 is reserved, data starts at page 1
  for (size_t pageIndex = 1; pageIndex < numPages.value(); pageIndex++) {
    Page *owned = nullptr;
    Page *page = nullptr;
    if (currentPage != nullptr && pageIndex == currentPageIndex) {
      page = currentPage;
    } else {
      auto read = readPage(file, pageIndex, tableName);
      if (!read) {
        return std::unexpected(read.error());
      }
      owned = read.value();
      page = owned;
    }

    uint16_t numSlots = page->numSlots();
    for (uint16_t slot = 0; slot < numSlots; slot++) {
      if (page->slotDeleted(slot)) {
        continue;
      }
      if (page == currentPage && skipSlot >= 0 &&
          slot == static_cast<uint16_t>(skipSlot)) {
        continue;
      }
      auto row = readSlotRow(page, slot, pageIndex, tableName, columns);
      if (!row) {
        delete owned;
        return std::unexpected(row.error());
      }
      if (row.value()[pkIndex] == value) {
        delete owned;
        return true;
      }
    }
    delete owned;
  }

  return false;
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
  // Page 0 is reserved, data starts at page 1
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
    return std::unexpected(ExecError::InternalError);
  }
  auto written = tableFile->writePage(*page);
  if (!written) {
    std::println(stderr, "[Executor] failed to write page {} of table '{}': {}",
                 page->pageId, tableName, tableFileErrorStr(written.error()));
    return std::unexpected(ExecError::InternalError);
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

  int pkIndex = primaryKeyIndex(schema->columns);
  if (pkIndex != -1) {
    auto exists =
        primaryKeyExists(file, parseResult.statement.tableName, schema->columns,
                         pkIndex, row.value()[pkIndex], nullptr, 0, -1);
    if (!exists) {
      return std::unexpected(exists.error());
    }
    if (exists.value()) {
      std::println(stderr,
                   "[Executor] duplicate primary key on insert into table '{}'",
                   parseResult.statement.tableName);
      return std::unexpected(ExecError::PrimaryKeyViolation);
    }
  }

  auto rowBytes = serializeRow(row.value(), schema->columns);
  if (!rowBytes) {
    std::println(
        stderr, "[Executor] failed to serialize row for table '{}': {}",
        parseResult.statement.tableName, rowErrorStr(rowBytes.error()));
    return std::unexpected(ExecError::InternalError);
  }

  auto numPages = getNumPages(file, parseResult.statement.tableName);
  if (!numPages) {
    return std::unexpected(numPages.error());
  }

  uint16_t rowSizeBytes = static_cast<uint16_t>(rowBytes.value().size());
  // Page 0 is reserved, data starts at page 1
  size_t pageIndex = 1;
  while (pageIndex < numPages.value()) {
    auto page = readPage(file, pageIndex, parseResult.statement.tableName);
    if (!page) {
      return std::unexpected(page.error());
    }
    if (page.value()->freeSpace() >= rowSizeBytes + SLOT_ENTRY_SIZE) {
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
  if (pageIndex >= numPages.value()) {
    auto newPageId = file->allocatePage();
    if (!newPageId) {
      std::println(stderr,
                   "[Executor] failed to allocate page for table '{}': {}",
                   parseResult.statement.tableName,
                   tableFileErrorStr(newPageId.error()));
      return std::unexpected(ExecError::InternalError);
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
  // Page 0 is reserved, data starts at page 1
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
          return std::unexpected(ExecError::InternalError);
        }
        deletedCount++;
      }
    }
    auto written = writePage(file, page.value(), pageIndex,
                             parseResult.statement.tableName);
    if (!written) {
      return std::unexpected(written.error());
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

  // Only worth checking when the statement actually assigns the primary key
  int pkIndex = primaryKeyIndex(schema->columns);
  bool pkAssigned = false;
  if (pkIndex != -1) {
    for (const auto &assignment : parseResult.statement.assignments) {
      if (assignment.first == schema->columns[pkIndex].name) {
        pkAssigned = true;
        break;
      }
    }
  }

  // Page 0 is reserved, data starts at page 1
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
        if (pkAssigned) {
          auto exists = primaryKeyExists(
              file, parseResult.statement.tableName, schema->columns, pkIndex,
              updated.value()[pkIndex], page.value(), pageIndex, slot);
          if (!exists) {
            return std::unexpected(exists.error());
          }
          if (exists.value()) {
            std::println(
                stderr,
                "[Executor] duplicate primary key on update of table '{}'",
                parseResult.statement.tableName);
            return std::unexpected(ExecError::PrimaryKeyViolation);
          }
        }
        auto rowBytes = serializeRow(updated.value(), schema->columns);
        if (!rowBytes) {
          std::println(
              stderr,
              "[Executor] failed to serialize updated row for table '{}'",
              parseResult.statement.tableName);
          return std::unexpected(ExecError::InternalError);
        }
        uint16_t rowSizeBytes = static_cast<uint16_t>(rowBytes.value().size());
        auto ok = page.value()->updateRow(slot, rowBytes.value().data(),
                                          rowSizeBytes);
        if (!ok) {
          std::println(stderr,
                       "[Executor] failed to update slot {} on page {} of "
                       "table '{}'",
                       slot, pageIndex, parseResult.statement.tableName);
          return std::unexpected(ExecError::InternalError);
        }
        updatedCount++;
      }
    }
    auto written = writePage(file, page.value(), pageIndex,
                             parseResult.statement.tableName);
    if (!written) {
      return std::unexpected(written.error());
    }
    pageIndex++;
  }

  return updatedCount;
}

std::expected<void, ExecError> execCreateTable(const ParseResult &parseResult,
                                               ExecutionContext &ctx) {
  Catalog &catalog = ctx.catalog;
  const std::string &tableName = parseResult.statement.tableName;

  TableSchema tableSchema;
  tableSchema.tableName = tableName;
  tableSchema.columns = parseResult.statement.columnDefinitions;
  tableSchema.filePath = std::format("{}.ms", tableName);
  TableFile *tableFile = new TableFile{};
  tableFile->filePath = tableSchema.filePath;
  auto created = tableFile->create();
  if (!created) {
    std::println(stderr, "[Executor] failed to create table '{}': {}",
                 tableName, tableFileErrorStr(created.error()));
    delete tableFile;
    return std::unexpected(ExecError::InternalError);
  }
  auto opened = tableFile->open(tableSchema.filePath);
  if (!opened) {
    std::println(stderr,
                 "[Executor] failed to open table '{}' after create: {}",
                 tableName, tableFileErrorStr(opened.error()));
    delete tableFile;
    return std::unexpected(ExecError::InternalError);
  }
  // Reserve page 0 so data pages start at index 1
  auto allocated = tableFile->allocatePage();
  if (!allocated) {
    std::println(stderr,
                 "[Executor] failed to allocate header page for table '{}': {}",
                 tableName, tableFileErrorStr(allocated.error()));
    delete tableFile;
    return std::unexpected(ExecError::InternalError);
  }
  auto result = catalog.addTable(tableSchema);
  if (!result) {
    std::println(stderr, "[Executor] failed to add table '{}': {}", tableName,
                 catalogErrorStr(result.error()));
    delete tableFile;
    return std::unexpected(ExecError::DuplicateTable);
  }
  catalog.save();
  ctx.openFiles[tableName] = tableFile;
  return {};
}

std::expected<void, ExecError> execDropTable(const ParseResult &parseResult,
                                             ExecutionContext &ctx) {
  Catalog &catalog = ctx.catalog;
  const std::string &tableName = parseResult.statement.tableName;

  if (!catalog.findTable(tableName)) {
    return std::unexpected(ExecError::TableSchemaNotFound);
  }

  auto it = ctx.openFiles.find(tableName);
  if (it != ctx.openFiles.end()) {
    it->second->close();
    delete it->second;
    ctx.openFiles.erase(it);
  }

  auto ok = catalog.dropTable(tableName);
  if (!ok) {
    return std::unexpected(ExecError::InternalError);
  }

  catalog.save();
  return {};
}

static Result makeError(ExecError err) {
  return Result{.success = false,
                .message = execErrorStr(err),
                .columns = {},
                .rows = {}};
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
    return Result{.success = true,
                  .message = "1 row inserted",
                  .columns = {},
                  .rows = {}};
  }
  case StatementKind::DELETE: {
    auto count = execDelete(parseResult, ctx);
    if (!count) {
      return makeError(count.error());
    }
    int n = count.value();
    return Result{.success = true,
                  .message =
                      std::format("{} row{} deleted", n, n == 1 ? "" : "s"),
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
                  .message =
                      std::format("{} row{} updated", n, n == 1 ? "" : "s"),
                  .columns = {},
                  .rows = {}};
  }
  case StatementKind::CREATE_TABLE: {
    auto ok = execCreateTable(parseResult, ctx);
    if (!ok) {
      return makeError(ok.error());
    }
    return Result{
        .success = true, .message = "Table created", .columns = {}, .rows = {}};
  }
  case StatementKind::DROP_TABLE: {
    auto ok = execDropTable(parseResult, ctx);
    if (!ok) {
      return makeError(ok.error());
    }
    return Result{
        .success = true, .message = "Table dropped", .columns = {}, .rows = {}};
  }
  case StatementKind::CREATE_INDEX:
  case StatementKind::BEGIN:
  case StatementKind::COMMIT:
  case StatementKind::ROLLBACK:
    break;
  }
  return makeError(ExecError::NotImplemented);
}
