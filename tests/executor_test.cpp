#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "ast/expression.h"
#include "ast/statement.h"
#include "catalog/tableschema.h"
#include "executor/executor.h"
#include "parser/parser.h"
#include "storage/page.h"
#include <doctest/doctest.h>
#include <unistd.h>

struct TempFile {
  std::string path;
  TempFile() {
    char buf[] = "/tmp/microsql_exec_test_XXXXXX";
    int fd = mkstemp(buf);
    if (fd != -1)
      ::close(fd);
    path = buf;
  }
  ~TempFile() { std::remove(path.c_str()); }
  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
};

static ColumnDefinition makeCol(const std::string &name, DataType type,
                                int textLength = 0, bool notNull = false) {
  ColumnDefinition col;
  col.name = name;
  col.type = type;
  col.textLength = textLength;
  col.notNull = notNull;
  return col;
}

struct TestCtx {
  TempFile tmp;
  TableFile *tf;
  ExecutionContext ctx;

  TestCtx(const std::string &tableName, std::vector<ColumnDefinition> cols) {
    tf = new TableFile{};
    REQUIRE(tf->open(tmp.path).has_value());
    REQUIRE(tf->allocatePage().has_value());

    TableSchema schema;
    schema.tableName = tableName;
    schema.columns = std::move(cols);
    ctx.catalog.addTable(schema);
    ctx.openFiles[tableName] = tf;
  }

  ~TestCtx() {
    tf->close();
    delete tf;
  }
};

static ParseResult makeSelectAll(const std::string &tableName) {
  ParseResult pr;
  pr.statement.kind = StatementKind::SELECT;
  pr.statement.tableName = tableName;
  pr.statement.whereIndex = -1;
  return pr;
}

static ParseResult makeInsert(const std::string &tableName,
                              std::vector<int64_t> values) {
  ParseResult pr;
  pr.statement.kind = StatementKind::INSERT;
  pr.statement.tableName = tableName;
  for (int64_t v : values) {
    Expression e;
    e.kind = ExpressionKind::LITERAL_INT;
    e.intValue = v;
    pr.statement.insertValues.push_back(pr.expressions.size());
    pr.expressions.push_back(e);
  }
  return pr;
}

static ParseResult makeSelectWhere(const std::string &tableName,
                                   const std::string &col, int64_t val) {
  ParseResult pr;
  pr.statement.kind = StatementKind::SELECT;
  pr.statement.tableName = tableName;
  pr.statement.whereIndex = 2;

  Expression colRef;
  colRef.kind = ExpressionKind::COLUMN_REF;
  colRef.columnName = col;
  pr.expressions.push_back(colRef);

  Expression lit;
  lit.kind = ExpressionKind::LITERAL_INT;
  lit.intValue = val;
  pr.expressions.push_back(lit);

  Expression binary;
  binary.kind = ExpressionKind::BINARY;
  binary.binaryOperator = BinaryOperator::EQUAL;
  binary.leftIndex = 0;
  binary.rightIndex = 1;
  pr.expressions.push_back(binary);

  return pr;
}

TEST_CASE("execInsert fails for unknown table") {
  ExecutionContext ctx;
  auto result = execute(makeInsert("ghost", {1, 2}), ctx);
  REQUIRE_FALSE(result.success);
  CHECK(result.message == "table not found");
}

TEST_CASE("execInsert fails when value count differs from schema") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  auto result = execute(makeInsert("users", {42}), tc.ctx);
  REQUIRE_FALSE(result.success);
  CHECK(result.message == "column count mismatch");
}

TEST_CASE("execInsert fails when explicit columns omit a NOT NULL column") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER, 0, true),
                        makeCol("age", DataType::INTEGER)});
  ParseResult pr;
  pr.statement.kind = StatementKind::INSERT;
  pr.statement.tableName = "users";
  pr.statement.insertColumnNames = {"age"};

  Expression e;
  e.kind = ExpressionKind::LITERAL_INT;
  e.intValue = 30;
  pr.expressions.push_back(e);
  pr.statement.insertValues = {0};

  auto result = execute(pr, tc.ctx);
  REQUIRE_FALSE(result.success);
  CHECK(result.message == "NOT NULL constraint violated");
}

TEST_CASE("execInsert succeeds with message") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  auto result = execute(makeInsert("users", {1, 25}), tc.ctx);
  REQUIRE(result.success);
  CHECK(result.message == "1 row inserted");
}

TEST_CASE("execInsert fills a page and allocates a new one") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  auto numBefore = tc.tf->numPages();
  REQUIRE(numBefore.has_value());

  int rowsPerPage = (PAGE_SIZE - HEADER_SIZE) / (8 + 8 + SLOT_ENTRY_SIZE);
  for (int i = 0; i < rowsPerPage + 1; i++) {
    REQUIRE(execute(makeInsert("users", {i, i * 2}), tc.ctx).success);
  }

  auto numAfter = tc.tf->numPages();
  REQUIRE(numAfter.has_value());
  CHECK(numAfter.value() > numBefore.value() + 1);
}

TEST_CASE("execSelect fails for unknown table") {
  ExecutionContext ctx;
  auto result = execute(makeSelectAll("ghost"), ctx);
  REQUIRE_FALSE(result.success);
  CHECK(result.message == "table not found");
}

TEST_CASE("execSelect on empty table returns no rows with correct columns") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  auto result = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(result.success);
  CHECK(result.rows.empty());
  CHECK(result.columns == std::vector<std::string>{"id", "age"});
}

TEST_CASE("execSelect returns row after insert") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {42, 25}), tc.ctx).success);

  auto result = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(result.success);
  REQUIRE(result.rows.size() == 1);
  CHECK(std::get<int64_t>(result.rows[0][0]) == 42);
  CHECK(std::get<int64_t>(result.rows[0][1]) == 25);
}

TEST_CASE("execSelect returns all inserted rows") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {2, 30}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {3, 40}), tc.ctx).success);

  auto result = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(result.success);
  CHECK(result.rows.size() == 3);
  CHECK(result.message == "3 rows");
}

TEST_CASE("execSelect with WHERE returns only matching rows") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {2, 30}), tc.ctx).success);

  auto result = execute(makeSelectWhere("users", "id", 1), tc.ctx);
  REQUIRE(result.success);
  REQUIRE(result.rows.size() == 1);
  CHECK(std::get<int64_t>(result.rows[0][0]) == 1);
}

TEST_CASE("execSelect with WHERE returns empty when no rows match") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);

  auto result = execute(makeSelectWhere("users", "id", 99), tc.ctx);
  REQUIRE(result.success);
  CHECK(result.rows.empty());
}
