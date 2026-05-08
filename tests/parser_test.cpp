#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "lexer/lexer.h"
#include "parser/parser.h"
#include <doctest/doctest.h>

static ParseResult parse(const std::string &query) {
  return ::parse(Lexer{query}.tokenize());
}

TEST_CASE("SELECT * FROM table") {
  auto result = parse("SELECT * FROM users;");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::SELECT);
  CHECK(result.statement.tableName == "users");
  REQUIRE(result.statement.selectColumns.size() == 1);
  CHECK(result.expressions[result.statement.selectColumns[0]].kind == ExpressionKind::STAR);
}

TEST_CASE("SELECT keywords are case-insensitive") {
  auto result = parse("select * from orders;");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::SELECT);
  CHECK(result.statement.tableName == "orders");
}

TEST_CASE("SELECT mixed-case keywords") {
  auto result = parse("Select * From Users;");
  CHECK(result.error.empty());
  CHECK(result.statement.tableName == "Users");
}

TEST_CASE("SELECT single column") {
  auto result = parse("SELECT id FROM users;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &col = result.expressions[result.statement.selectColumns[0]];
  CHECK(col.kind == ExpressionKind::COLUMN_REF);
  CHECK(col.columnName == "id");
  CHECK(col.tablePrefix.empty());
}

TEST_CASE("SELECT multiple columns") {
  auto result = parse("SELECT id, name, age FROM users;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 3);
  CHECK(result.expressions[result.statement.selectColumns[0]].columnName == "id");
  CHECK(result.expressions[result.statement.selectColumns[1]].columnName == "name");
  CHECK(result.expressions[result.statement.selectColumns[2]].columnName == "age");
}

TEST_CASE("SELECT qualified column") {
  auto result = parse("SELECT users.id FROM users;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &col = result.expressions[result.statement.selectColumns[0]];
  CHECK(col.kind == ExpressionKind::COLUMN_REF);
  CHECK(col.tablePrefix == "users");
  CHECK(col.columnName == "id");
}

TEST_CASE("SELECT integer literal") {
  auto result = parse("SELECT 42;");
  CHECK(result.error.empty());
  CHECK(result.statement.tableName.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::LITERAL_INT);
  CHECK(expr.intValue == 42);
}

TEST_CASE("SELECT string literal") {
  auto result = parse("SELECT 'hello';");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::LITERAL_TEXT);
  CHECK(expr.textValue == "hello");
}

TEST_CASE("SELECT arithmetic expression") {
  auto result = parse("SELECT 1 + 2;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::ADD);
  CHECK(result.expressions[expr.leftIndex].intValue == 1);
  CHECK(result.expressions[expr.rightIndex].intValue == 2);
}

TEST_CASE("SELECT without FROM is valid") {
  auto result = parse("SELECT 1 + 1;");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::SELECT);
  CHECK(result.statement.tableName.empty());
}

TEST_CASE("SELECT WHERE integer equality") {
  auto result = parse("SELECT * FROM users WHERE id = 1;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::EQUAL);
  CHECK(result.expressions[where.leftIndex].columnName == "id");
  CHECK(result.expressions[where.rightIndex].intValue == 1);
}

TEST_CASE("SELECT WHERE string equality") {
  auto result = parse("SELECT * FROM users WHERE name = 'Alice';");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.binaryOperator == BinaryOperator::EQUAL);
  auto &rhs = result.expressions[where.rightIndex];
  CHECK(rhs.kind == ExpressionKind::LITERAL_TEXT);
  CHECK(rhs.textValue == "Alice");
}

TEST_CASE("SELECT WHERE greater-than") {
  auto result = parse("SELECT * FROM users WHERE age > 18;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.binaryOperator == BinaryOperator::GREATER_THAN);
}

TEST_CASE("SELECT WHERE less-than") {
  auto result = parse("SELECT * FROM users WHERE age < 65;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::LESS_THAN);
}

TEST_CASE("SELECT WHERE greater-than-or-equal") {
  auto result = parse("SELECT * FROM users WHERE score >= 50;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::GREATER_THAN_OR_EQUAL);
}

TEST_CASE("SELECT WHERE less-than-or-equal") {
  auto result = parse("SELECT * FROM scores WHERE value <= 100;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::LESS_THAN_OR_EQUAL);
}

TEST_CASE("SELECT WHERE not-equal") {
  auto result = parse("SELECT * FROM users WHERE status != 'inactive';");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::NOT_EQUAL);
}

TEST_CASE("SELECT WHERE AND") {
  auto result = parse("SELECT * FROM users WHERE age > 18 AND age < 65;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::AND);
}

TEST_CASE("SELECT WHERE OR") {
  auto result = parse("SELECT * FROM users WHERE role = 'admin' OR role = 'mod';");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::OR);
}

TEST_CASE("SELECT WHERE NOT") {
  auto result = parse("SELECT * FROM users WHERE NOT active;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.kind == ExpressionKind::UNARY);
  CHECK(where.unaryOperator == UnaryOperator::NOT);
}

TEST_CASE("SELECT WHERE boolean literal") {
  auto result = parse("SELECT * FROM users WHERE active = true;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  auto &rhs = result.expressions[result.expressions[result.statement.whereIndex].rightIndex];
  CHECK(rhs.kind == ExpressionKind::LITERAL_BOOL);
  CHECK(rhs.boolValue == true);
}

TEST_CASE("SELECT WHERE IS NULL") {
  auto result = parse("SELECT * FROM users WHERE deleted_at IS NULL;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(result.expressions[where.rightIndex].kind == ExpressionKind::LITERAL_NULL);
}

TEST_CASE("SELECT WHERE IS NOT NULL") {
  auto result = parse("SELECT * FROM users WHERE email IS NOT NULL;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
}

TEST_CASE("SELECT missing FROM produces error") {
  auto result = parse("SELECT * users;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("SELECT error message mentions unexpected token") {
  auto result = parse("SELECT * users;");
  CHECK(result.error.find("users") != std::string::npos);
}

TEST_CASE("SELECT missing table name after FROM") {
  auto result = parse("SELECT * FROM;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("SELECT incomplete WHERE clause") {
  auto result = parse("SELECT * FROM users WHERE;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("SELECT WHERE with incomplete expression") {
  auto result = parse("SELECT * FROM users WHERE id =;");
  CHECK_FALSE(result.error.empty());
}
