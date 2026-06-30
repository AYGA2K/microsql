#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "ast/expression.h"
#include "ast/statement.h"
#include "catalog/tableschema.h"
#include "executor/executor.h"
#include "parser/parser.h"
#include "storage/page.h"
#include <doctest/doctest.h>
#include <fstream>
#include <unistd.h>

struct TempTableFile {
  std::string path;
  TempTableFile(const std::string &tableName) : path(tableName + ".ms") {}
  ~TempTableFile() { std::remove(path.c_str()); }
  TempTableFile(const TempTableFile &) = delete;
  TempTableFile &operator=(const TempTableFile &) = delete;
};

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
    REQUIRE(ctx.catalog.addTable(schema).has_value());
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

static ParseResult makeDeleteAll(const std::string &tableName) {
  ParseResult pr;
  pr.statement.kind = StatementKind::DELETE;
  pr.statement.tableName = tableName;
  pr.statement.whereIndex = -1;
  return pr;
}

static ParseResult makeDeleteWhere(const std::string &tableName,
                                   const std::string &col, int64_t val) {
  ParseResult pr;
  pr.statement.kind = StatementKind::DELETE;
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

static ParseResult makeUpdateAll(const std::string &tableName,
                                 const std::string &setCol, int64_t setVal) {
  ParseResult pr;
  pr.statement.kind = StatementKind::UPDATE;
  pr.statement.tableName = tableName;
  pr.statement.whereIndex = -1;

  Expression lit;
  lit.kind = ExpressionKind::LITERAL_INT;
  lit.intValue = setVal;
  pr.expressions.push_back(lit);
  pr.statement.assignments.push_back({setCol, 0});

  return pr;
}

static ParseResult makeUpdateWhere(const std::string &tableName,
                                   const std::string &setCol, int64_t setVal,
                                   const std::string &whereCol,
                                   int64_t whereVal) {
  ParseResult pr;
  pr.statement.kind = StatementKind::UPDATE;
  pr.statement.tableName = tableName;

  Expression setLit;
  setLit.kind = ExpressionKind::LITERAL_INT;
  setLit.intValue = setVal;
  pr.expressions.push_back(setLit);       // index 0: SET value
  pr.statement.assignments.push_back({setCol, 0});

  Expression colRef;
  colRef.kind = ExpressionKind::COLUMN_REF;
  colRef.columnName = whereCol;
  pr.expressions.push_back(colRef);       // index 1: WHERE column

  Expression whereLit;
  whereLit.kind = ExpressionKind::LITERAL_INT;
  whereLit.intValue = whereVal;
  pr.expressions.push_back(whereLit);     // index 2: WHERE value

  Expression binary;
  binary.kind = ExpressionKind::BINARY;
  binary.binaryOperator = BinaryOperator::EQUAL;
  binary.leftIndex = 1;
  binary.rightIndex = 2;
  pr.expressions.push_back(binary);       // index 3: WHERE clause
  pr.statement.whereIndex = 3;

  return pr;
}

static ParseResult makeInsertText(const std::string &tableName,
                                  int64_t id, const std::string &text) {
  ParseResult pr;
  pr.statement.kind = StatementKind::INSERT;
  pr.statement.tableName = tableName;

  Expression idExpr;
  idExpr.kind = ExpressionKind::LITERAL_INT;
  idExpr.intValue = id;
  pr.statement.insertValues.push_back(pr.expressions.size());
  pr.expressions.push_back(idExpr);

  Expression textExpr;
  textExpr.kind = ExpressionKind::LITERAL_TEXT;
  textExpr.textValue = text;
  pr.statement.insertValues.push_back(pr.expressions.size());
  pr.expressions.push_back(textExpr);

  return pr;
}

static ParseResult makeCreateTable(const std::string &tableName,
                                   std::vector<ColumnDefinition> cols) {
  ParseResult pr;
  pr.statement.kind = StatementKind::CREATE_TABLE;
  pr.statement.tableName = tableName;
  pr.statement.columnDefinitions = std::move(cols);
  return pr;
}

TEST_CASE("execCreateTable succeeds with correct message") {
  TempTableFile cleanup("employees");
  ExecutionContext ctx;
  auto result = execute(
      makeCreateTable("employees",
                      {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)}),
      ctx);
  REQUIRE(result.success);
  CHECK(result.message == "Table created");
}

TEST_CASE("execCreateTable registers schema in catalog") {
  TempTableFile cleanup("employees");
  ExecutionContext ctx;
  REQUIRE(execute(makeCreateTable("employees",
                                  {makeCol("id", DataType::INTEGER),
                                   makeCol("age", DataType::INTEGER)}),
                  ctx)
              .success);
  TableSchema *schema = ctx.catalog.findTable("employees");
  REQUIRE(schema != nullptr);
  CHECK(schema->tableName == "employees");
  REQUIRE(schema->columns.size() == 2);
  CHECK(schema->columns[0].name == "id");
  CHECK(schema->columns[1].name == "age");
  CHECK(schema->filePath == "employees.ms");
}

TEST_CASE("execCreateTable creates .ms file on disk") {
  TempTableFile cleanup("employees");
  ExecutionContext ctx;
  REQUIRE(execute(makeCreateTable("employees", {makeCol("id", DataType::INTEGER)}), ctx).success);
  std::ifstream f("employees.ms");
  CHECK(f.good());
}

TEST_CASE("execCreateTable fails on duplicate table") {
  TempTableFile cleanup("employees");
  ExecutionContext ctx;
  REQUIRE(execute(makeCreateTable("employees", {makeCol("id", DataType::INTEGER)}), ctx).success);
  auto result = execute(makeCreateTable("employees", {makeCol("id", DataType::INTEGER)}), ctx);
  REQUIRE_FALSE(result.success);
  CHECK(result.message == "duplicate table");
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

TEST_CASE("execDelete fails for unknown table") {
  ExecutionContext ctx;
  auto result = execute(makeDeleteAll("ghost"), ctx);
  REQUIRE_FALSE(result.success);
  CHECK(result.message == "table not found");
}

TEST_CASE("execDelete without WHERE removes all rows") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {2, 30}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {3, 40}), tc.ctx).success);

  auto del = execute(makeDeleteAll("users"), tc.ctx);
  REQUIRE(del.success);
  CHECK(del.message == "3 rows deleted");

  auto sel = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(sel.success);
  CHECK(sel.rows.empty());
}

TEST_CASE("execDelete with WHERE removes only matching rows") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {2, 30}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {3, 40}), tc.ctx).success);

  auto del = execute(makeDeleteWhere("users", "id", 2), tc.ctx);
  REQUIRE(del.success);
  CHECK(del.message == "1 row deleted");

  auto sel = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(sel.success);
  REQUIRE(sel.rows.size() == 2);
  CHECK(std::get<int64_t>(sel.rows[0][0]) == 1);
  CHECK(std::get<int64_t>(sel.rows[1][0]) == 3);
}

TEST_CASE("execDelete with WHERE that matches nothing deletes zero rows") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);

  auto del = execute(makeDeleteWhere("users", "id", 99), tc.ctx);
  REQUIRE(del.success);
  CHECK(del.message == "0 rows deleted");

  auto sel = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(sel.success);
  CHECK(sel.rows.size() == 1);
}

TEST_CASE("execUpdate fails for unknown table") {
  ExecutionContext ctx;
  auto result = execute(makeUpdateAll("ghost", "age", 99), ctx);
  REQUIRE_FALSE(result.success);
  CHECK(result.message == "table not found");
}

TEST_CASE("execUpdate without WHERE updates all rows") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {2, 30}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {3, 40}), tc.ctx).success);

  auto upd = execute(makeUpdateAll("users", "age", 99), tc.ctx);
  REQUIRE(upd.success);
  CHECK(upd.message == "3 rows updated");

  auto sel = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(sel.success);
  REQUIRE(sel.rows.size() == 3);
  CHECK(std::get<int64_t>(sel.rows[0][1]) == 99);
  CHECK(std::get<int64_t>(sel.rows[1][1]) == 99);
  CHECK(std::get<int64_t>(sel.rows[2][1]) == 99);
}

TEST_CASE("execUpdate with WHERE updates only matching rows") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {2, 30}), tc.ctx).success);
  REQUIRE(execute(makeInsert("users", {3, 40}), tc.ctx).success);

  auto upd = execute(makeUpdateWhere("users", "age", 99, "id", 2), tc.ctx);
  REQUIRE(upd.success);
  CHECK(upd.message == "1 row updated");

  auto sel = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(sel.success);
  REQUIRE(sel.rows.size() == 3);
  CHECK(std::get<int64_t>(sel.rows[0][1]) == 20);
  CHECK(std::get<int64_t>(sel.rows[1][1]) == 99);
  CHECK(std::get<int64_t>(sel.rows[2][1]) == 40);
}

TEST_CASE("execUpdate with WHERE that matches nothing updates zero rows") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER), makeCol("age", DataType::INTEGER)});
  REQUIRE(execute(makeInsert("users", {1, 20}), tc.ctx).success);

  auto upd = execute(makeUpdateWhere("users", "age", 99, "id", 42), tc.ctx);
  REQUIRE(upd.success);
  CHECK(upd.message == "0 rows updated");

  auto sel = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(sel.success);
  REQUIRE(sel.rows.size() == 1);
  CHECK(std::get<int64_t>(sel.rows[0][1]) == 20);
}

TEST_CASE("execSelect TEXT column roundtrip preserves value exactly") {
  TestCtx tc("users", {makeCol("id", DataType::INTEGER),
                        makeCol("name", DataType::TEXT, 50)});
  REQUIRE(execute(makeInsertText("users", 1, "Alice"), tc.ctx).success);
  REQUIRE(execute(makeInsertText("users", 2, "Bob"), tc.ctx).success);

  auto sel = execute(makeSelectAll("users"), tc.ctx);
  REQUIRE(sel.success);
  REQUIRE(sel.rows.size() == 2);
  CHECK(std::get<int64_t>(sel.rows[0][0]) == 1);
  CHECK(std::get<std::string>(sel.rows[0][1]) == "Alice");
  CHECK(std::get<int64_t>(sel.rows[1][0]) == 2);
  CHECK(std::get<std::string>(sel.rows[1][1]) == "Bob");
}

TEST_CASE("CREATE TABLE then INSERT then SELECT returns correct rows") {
  TempTableFile cleanup("users");
  ExecutionContext ctx;

  REQUIRE(execute(makeCreateTable("users", {makeCol("id", DataType::INTEGER),
                                            makeCol("name", DataType::TEXT, 50)}),
                  ctx).success);
  REQUIRE(execute(makeInsertText("users", 1, "Alice"), ctx).success);
  REQUIRE(execute(makeInsertText("users", 2, "Bob"), ctx).success);

  auto sel = execute(makeSelectAll("users"), ctx);
  REQUIRE(sel.success);
  REQUIRE(sel.rows.size() == 2);
  CHECK(std::get<int64_t>(sel.rows[0][0]) == 1);
  CHECK(std::get<std::string>(sel.rows[0][1]) == "Alice");
  CHECK(std::get<int64_t>(sel.rows[1][0]) == 2);
  CHECK(std::get<std::string>(sel.rows[1][1]) == "Bob");
}
