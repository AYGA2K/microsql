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
  CHECK(result.statement.whereIndex == -1);
  REQUIRE(result.statement.selectColumns.size() == 0);
}

TEST_CASE("SELECT single column") {
  auto result = parse("SELECT id FROM users;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex == -1);
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
  CHECK(result.expressions[result.statement.selectColumns[0]].columnName ==
        "id");
  CHECK(result.expressions[result.statement.selectColumns[1]].columnName ==
        "name");
  CHECK(result.expressions[result.statement.selectColumns[2]].columnName ==
        "age");
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

TEST_CASE("SELECT addition expression") {
  auto result = parse("SELECT 1 + 2;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::ADD);
  CHECK(result.expressions[expr.leftIndex].intValue == 1);
  CHECK(result.expressions[expr.rightIndex].intValue == 2);
}

TEST_CASE("SELECT subtract expression") {
  auto result = parse("SELECT 1 - 5;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::SUBTRACT);
  CHECK(result.expressions[expr.leftIndex].intValue == 1);
  CHECK(result.expressions[expr.rightIndex].intValue == 5);
}

TEST_CASE("SELECT multiply expression") {
  auto result = parse("SELECT 1 * 6;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::MULTIPLY);
  CHECK(result.expressions[expr.leftIndex].intValue == 1);
  CHECK(result.expressions[expr.rightIndex].intValue == 6);
}

TEST_CASE("SELECT divide expression") {
  auto result = parse("SELECT 9 / 4;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::DIVIDE);
  CHECK(result.expressions[expr.leftIndex].intValue == 9);
  CHECK(result.expressions[expr.rightIndex].intValue == 4);
}

TEST_CASE("SELECT negate integer literal") {
  auto result = parse("SELECT -5;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  REQUIRE(expr.kind == ExpressionKind::UNARY);
  CHECK(expr.unaryOperator == UnaryOperator::NEGATE);
  CHECK(result.expressions[expr.operandIndex].kind ==
        ExpressionKind::LITERAL_INT);
  CHECK(result.expressions[expr.operandIndex].intValue == 5);
}

TEST_CASE("SELECT negate column") {
  auto result = parse("SELECT -age FROM users;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  REQUIRE(expr.kind == ExpressionKind::UNARY);
  CHECK(expr.unaryOperator == UnaryOperator::NEGATE);
  CHECK(result.expressions[expr.operandIndex].kind ==
        ExpressionKind::COLUMN_REF);
  CHECK(result.expressions[expr.operandIndex].columnName == "age");
}

TEST_CASE("SELECT negate in arithmetic") {
  auto result = parse("SELECT -3 + 4;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  REQUIRE(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::ADD);
  auto &lhs = result.expressions[expr.leftIndex];
  REQUIRE(lhs.kind == ExpressionKind::UNARY);
  CHECK(lhs.unaryOperator == UnaryOperator::NEGATE);
  CHECK(result.expressions[lhs.operandIndex].intValue == 3);
  CHECK(result.expressions[expr.rightIndex].intValue == 4);
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
  auto result =
      parse("SELECT * FROM users WHERE role = 'admin' OR role = 'mod';");
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
  auto &rhs = result.expressions[result.expressions[result.statement.whereIndex]
                                     .rightIndex];
  CHECK(rhs.kind == ExpressionKind::LITERAL_BOOL);
  CHECK(rhs.boolValue == true);
}

TEST_CASE("SELECT WHERE IS NULL") {
  auto result = parse("SELECT * FROM users WHERE deleted_at IS NULL;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  REQUIRE(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::IS);
  CHECK(result.expressions[where.leftIndex].kind == ExpressionKind::COLUMN_REF);
  CHECK(result.expressions[where.leftIndex].columnName == "deleted_at");
  CHECK(result.expressions[where.rightIndex].kind ==
        ExpressionKind::LITERAL_NULL);
}

TEST_CASE("SELECT WHERE IS NOT NULL") {
  auto result = parse("SELECT * FROM users WHERE email IS NOT NULL;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  REQUIRE(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::IS_NOT);
  CHECK(result.expressions[where.leftIndex].kind == ExpressionKind::COLUMN_REF);
  CHECK(result.expressions[where.leftIndex].columnName == "email");
  CHECK(result.expressions[where.rightIndex].kind ==
        ExpressionKind::LITERAL_NULL);
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

TEST_CASE("SELECT float literal") {
  auto result = parse("SELECT 3.14;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::LITERAL_FLOAT);
  CHECK(expr.floatValue == doctest::Approx(3.14));
}

TEST_CASE("SELECT NULL literal") {
  auto result = parse("SELECT NULL;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  CHECK(expr.kind == ExpressionKind::LITERAL_NULL);
}

TEST_CASE("SELECT WHERE false literal") {
  auto result = parse("SELECT * FROM users WHERE active = false;");
  CHECK(result.error.empty());
  CHECK(result.statement.whereIndex != -1);
  auto &rhs = result.expressions[result.expressions[result.statement.whereIndex]
                                     .rightIndex];
  CHECK(rhs.kind == ExpressionKind::LITERAL_BOOL);
  CHECK(rhs.boolValue == false);
}

TEST_CASE("SELECT arithmetic precedence multiply before add") {
  auto result = parse("SELECT 2 + 3 * 4;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  REQUIRE(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::ADD);
  CHECK(result.expressions[expr.leftIndex].intValue == 2);
  auto &rhs = result.expressions[expr.rightIndex];
  REQUIRE(rhs.kind == ExpressionKind::BINARY);
  CHECK(rhs.binaryOperator == BinaryOperator::MULTIPLY);
  CHECK(result.expressions[rhs.leftIndex].intValue == 3);
  CHECK(result.expressions[rhs.rightIndex].intValue == 4);
}

TEST_CASE("SELECT WHERE AND binds tighter than OR") {
  auto result = parse("SELECT * FROM users WHERE a AND b OR c;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  REQUIRE(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::OR);
  auto &lhs = result.expressions[where.leftIndex];
  REQUIRE(lhs.kind == ExpressionKind::BINARY);
  CHECK(lhs.binaryOperator == BinaryOperator::AND);
  CHECK(result.expressions[lhs.leftIndex].columnName == "a");
  CHECK(result.expressions[lhs.rightIndex].columnName == "b");
  CHECK(result.expressions[where.rightIndex].columnName == "c");
}

TEST_CASE("SELECT WHERE arithmetic in condition") {
  auto result = parse("SELECT * FROM users WHERE age + 1 > 18;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  REQUIRE(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::GREATER_THAN);
  auto &lhs = result.expressions[where.leftIndex];
  REQUIRE(lhs.kind == ExpressionKind::BINARY);
  CHECK(lhs.binaryOperator == BinaryOperator::ADD);
  CHECK(result.expressions[lhs.leftIndex].columnName == "age");
  CHECK(result.expressions[lhs.rightIndex].intValue == 1);
  CHECK(result.expressions[where.rightIndex].intValue == 18);
}

TEST_CASE("SELECT WHERE qualified column") {
  auto result = parse("SELECT * FROM users WHERE users.id = 1;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  auto &lhs = result.expressions[where.leftIndex];
  CHECK(lhs.kind == ExpressionKind::COLUMN_REF);
  CHECK(lhs.tablePrefix == "users");
  CHECK(lhs.columnName == "id");
}

TEST_CASE("SELECT without semicolon") {
  auto result = parse("SELECT * FROM users");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::SELECT);
  CHECK(result.statement.tableName == "users");
}

TEST_CASE("SELECT bad dot notation produces error") {
  auto result = parse("SELECT users. FROM users;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("SELECT parenthesized expression") {
  auto result = parse("SELECT (1 + 2);");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  REQUIRE(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::ADD);
  CHECK(result.expressions[expr.leftIndex].intValue == 1);
  CHECK(result.expressions[expr.rightIndex].intValue == 2);
}

TEST_CASE("SELECT parentheses override precedence") {
  auto result = parse("SELECT (1 + 2) * 3;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.selectColumns.size() == 1);
  auto &expr = result.expressions[result.statement.selectColumns[0]];
  REQUIRE(expr.kind == ExpressionKind::BINARY);
  CHECK(expr.binaryOperator == BinaryOperator::MULTIPLY);
  auto &lhs = result.expressions[expr.leftIndex];
  REQUIRE(lhs.kind == ExpressionKind::BINARY);
  CHECK(lhs.binaryOperator == BinaryOperator::ADD);
  CHECK(result.expressions[lhs.leftIndex].intValue == 1);
  CHECK(result.expressions[lhs.rightIndex].intValue == 2);
  CHECK(result.expressions[expr.rightIndex].intValue == 3);
}

TEST_CASE("SELECT WHERE parentheses override AND/OR precedence") {
  auto result = parse("SELECT * FROM t WHERE (a = 1 OR b = 2) AND c = 3;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  REQUIRE(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::AND);
  auto &lhs = result.expressions[where.leftIndex];
  REQUIRE(lhs.kind == ExpressionKind::BINARY);
  CHECK(lhs.binaryOperator == BinaryOperator::OR);
}

TEST_CASE("SELECT unclosed parenthesis produces error") {
  auto result = parse("SELECT (1 + 2;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("INSERT INTO table VALUES single integer") {
  auto result = parse("INSERT INTO users VALUES (1);");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::INSERT);
  CHECK(result.statement.tableName == "users");
  REQUIRE(result.statement.insertValues.size() == 1);
  CHECK(result.expressions[result.statement.insertValues[0]].kind ==
        ExpressionKind::LITERAL_INT);
  CHECK(result.expressions[result.statement.insertValues[0]].intValue == 1);
}

TEST_CASE("INSERT INTO table VALUES multiple values") {
  auto result = parse("INSERT INTO users VALUES (42, 'Alice', 3.14);");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::INSERT);
  CHECK(result.statement.tableName == "users");
  REQUIRE(result.statement.insertValues.size() == 3);
  CHECK(result.expressions[result.statement.insertValues[0]].intValue == 42);
  CHECK(result.expressions[result.statement.insertValues[1]].textValue ==
        "Alice");
  CHECK(result.expressions[result.statement.insertValues[2]].kind ==
        ExpressionKind::LITERAL_FLOAT);
}

TEST_CASE("INSERT INTO table with column list") {
  auto result = parse("INSERT INTO users (id, name) VALUES (1, 'Bob');");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::INSERT);
  CHECK(result.statement.tableName == "users");
  REQUIRE(result.statement.insertColumnNames.size() == 2);
  CHECK(result.statement.insertColumnNames[0] == "id");
  CHECK(result.statement.insertColumnNames[1] == "name");
  REQUIRE(result.statement.insertValues.size() == 2);
  CHECK(result.expressions[result.statement.insertValues[0]].intValue == 1);
  CHECK(result.expressions[result.statement.insertValues[1]].textValue ==
        "Bob");
}

TEST_CASE("INSERT INTO table VALUES NULL") {
  auto result = parse("INSERT INTO users VALUES (NULL);");
  CHECK(result.error.empty());
  REQUIRE(result.statement.insertValues.size() == 1);
  CHECK(result.expressions[result.statement.insertValues[0]].kind ==
        ExpressionKind::LITERAL_NULL);
}

TEST_CASE("INSERT missing INTO produces error") {
  auto result = parse("INSERT users VALUES (1);");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("INSERT missing table name produces error") {
  auto result = parse("INSERT INTO VALUES (1);");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("INSERT missing VALUES keyword produces error") {
  auto result = parse("INSERT INTO users (1);");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("DELETE FROM table no WHERE") {
  auto result = parse("DELETE FROM users;");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::DELETE);
  CHECK(result.statement.tableName == "users");
  CHECK(result.statement.whereIndex == -1);
}

TEST_CASE("DELETE missing semicolon produces error") {
  auto result = parse("DELETE FROM users");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("DELETE FROM table WHERE integer equality") {
  auto result = parse("DELETE FROM users WHERE id = 1;");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::DELETE);
  CHECK(result.statement.tableName == "users");
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::EQUAL);
  CHECK(result.expressions[where.leftIndex].columnName == "id");
  CHECK(result.expressions[where.rightIndex].intValue == 1);
}

TEST_CASE("DELETE FROM table WHERE string equality") {
  auto result = parse("DELETE FROM users WHERE name = 'Alice';");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::DELETE);
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.binaryOperator == BinaryOperator::EQUAL);
  auto &rhs = result.expressions[where.rightIndex];
  CHECK(rhs.kind == ExpressionKind::LITERAL_TEXT);
  CHECK(rhs.textValue == "Alice");
}

TEST_CASE("DELETE FROM table WHERE greater-than") {
  auto result = parse("DELETE FROM logs WHERE age > 30;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::GREATER_THAN);
}

TEST_CASE("DELETE FROM table WHERE less-than") {
  auto result = parse("DELETE FROM logs WHERE score < 10;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::LESS_THAN);
}

TEST_CASE("DELETE FROM table WHERE boolean literal") {
  auto result = parse("DELETE FROM users WHERE active = false;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &rhs = result.expressions[result.expressions[result.statement.whereIndex]
                                     .rightIndex];
  CHECK(rhs.kind == ExpressionKind::LITERAL_BOOL);
  CHECK(rhs.boolValue == false);
}

TEST_CASE("DELETE FROM table WHERE IS NULL") {
  auto result = parse("DELETE FROM users WHERE deleted_at IS NULL;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.binaryOperator == BinaryOperator::IS);
  CHECK(result.expressions[where.leftIndex].columnName == "deleted_at");
  CHECK(result.expressions[where.rightIndex].kind ==
        ExpressionKind::LITERAL_NULL);
}

TEST_CASE("DELETE FROM table WHERE IS NOT NULL") {
  auto result = parse("DELETE FROM users WHERE email IS NOT NULL;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.binaryOperator == BinaryOperator::IS_NOT);
  CHECK(result.expressions[where.leftIndex].columnName == "email");
  CHECK(result.expressions[where.rightIndex].kind ==
        ExpressionKind::LITERAL_NULL);
}

TEST_CASE("DELETE FROM table WHERE AND") {
  auto result = parse("DELETE FROM users WHERE age > 18 AND active = true;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::AND);
}

TEST_CASE("DELETE FROM table WHERE OR") {
  auto result =
      parse("DELETE FROM users WHERE role = 'guest' OR role = 'banned';");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::OR);
}

TEST_CASE("DELETE FROM table WHERE NOT") {
  auto result = parse("DELETE FROM users WHERE NOT active;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.kind == ExpressionKind::UNARY);
  CHECK(where.unaryOperator == UnaryOperator::NOT);
}

TEST_CASE("DELETE missing FROM produces error") {
  auto result = parse("DELETE users;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("DELETE missing table name after FROM produces error") {
  auto result = parse("DELETE FROM;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("DELETE missing table name before WHERE produces error") {
  auto result = parse("DELETE FROM WHERE id = 1;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("DELETE incomplete WHERE clause produces error") {
  auto result = parse("DELETE FROM users WHERE;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("DELETE WHERE with incomplete expression produces error") {
  auto result = parse("DELETE FROM users WHERE id =;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("UPDATE single integer assignment") {
  auto result = parse("UPDATE users SET age = 30;");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::UPDATE);
  CHECK(result.statement.tableName == "users");
  REQUIRE(result.statement.assignments.size() == 1);
  CHECK(result.statement.assignments[0].first == "age");
  auto &val = result.expressions[result.statement.assignments[0].second];
  CHECK(val.kind == ExpressionKind::LITERAL_INT);
  CHECK(val.intValue == 30);
  CHECK(result.statement.whereIndex == -1);
}

TEST_CASE("UPDATE single string assignment") {
  auto result = parse("UPDATE users SET name = 'Alice';");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::UPDATE);
  CHECK(result.statement.tableName == "users");
  REQUIRE(result.statement.assignments.size() == 1);
  CHECK(result.statement.assignments[0].first == "name");
  auto &val = result.expressions[result.statement.assignments[0].second];
  CHECK(val.kind == ExpressionKind::LITERAL_TEXT);
  CHECK(val.textValue == "Alice");
}

TEST_CASE("UPDATE single NULL assignment") {
  auto result = parse("UPDATE users SET deleted_at = NULL;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.assignments.size() == 1);
  CHECK(result.statement.assignments[0].first == "deleted_at");
  auto &val = result.expressions[result.statement.assignments[0].second];
  CHECK(val.kind == ExpressionKind::LITERAL_NULL);
}

TEST_CASE("UPDATE single boolean assignment") {
  auto result = parse("UPDATE users SET active = true;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.assignments.size() == 1);
  CHECK(result.statement.assignments[0].first == "active");
  auto &val = result.expressions[result.statement.assignments[0].second];
  CHECK(val.kind == ExpressionKind::LITERAL_BOOL);
  CHECK(val.boolValue == true);
}

TEST_CASE("UPDATE assignment with arithmetic expression") {
  auto result = parse("UPDATE products SET price = price + 10;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.assignments.size() == 1);
  CHECK(result.statement.assignments[0].first == "price");
  auto &val = result.expressions[result.statement.assignments[0].second];
  REQUIRE(val.kind == ExpressionKind::BINARY);
  CHECK(val.binaryOperator == BinaryOperator::ADD);
  CHECK(result.expressions[val.leftIndex].columnName == "price");
  CHECK(result.expressions[val.rightIndex].intValue == 10);
}

TEST_CASE("UPDATE multiple assignments") {
  auto result = parse("UPDATE users SET name = 'Bob', age = 25;");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::UPDATE);
  CHECK(result.statement.tableName == "users");
  REQUIRE(result.statement.assignments.size() == 2);
  CHECK(result.statement.assignments[0].first == "name");
  CHECK(result.expressions[result.statement.assignments[0].second].textValue ==
        "Bob");
  CHECK(result.statement.assignments[1].first == "age");
  CHECK(result.expressions[result.statement.assignments[1].second].intValue ==
        25);
}

TEST_CASE("UPDATE three assignments") {
  auto result =
      parse("UPDATE users SET name = 'Carol', age = 40, active = false;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.assignments.size() == 3);
  CHECK(result.statement.assignments[0].first == "name");
  CHECK(result.statement.assignments[1].first == "age");
  CHECK(result.statement.assignments[2].first == "active");
  CHECK(result.expressions[result.statement.assignments[2].second].boolValue ==
        false);
}

TEST_CASE("UPDATE with WHERE integer equality") {
  auto result = parse("UPDATE users SET age = 30 WHERE id = 1;");
  CHECK(result.error.empty());
  CHECK(result.statement.kind == StatementKind::UPDATE);
  REQUIRE(result.statement.assignments.size() == 1);
  CHECK(result.statement.assignments[0].first == "age");
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::EQUAL);
  CHECK(result.expressions[where.leftIndex].columnName == "id");
  CHECK(result.expressions[where.rightIndex].intValue == 1);
}

TEST_CASE("UPDATE with WHERE string equality") {
  auto result = parse("UPDATE users SET active = false WHERE name = 'Alice';");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.binaryOperator == BinaryOperator::EQUAL);
  CHECK(result.expressions[where.rightIndex].textValue == "Alice");
}

TEST_CASE("UPDATE multiple assignments with WHERE") {
  auto result = parse("UPDATE users SET name = 'Dave', age = 22 WHERE id = 5;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.assignments.size() == 2);
  CHECK(result.statement.assignments[0].first == "name");
  CHECK(result.statement.assignments[1].first == "age");
  REQUIRE(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::EQUAL);
}

TEST_CASE("UPDATE with WHERE AND") {
  auto result = parse(
      "UPDATE users SET active = false WHERE age < 18 AND role = 'guest';");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.kind == ExpressionKind::BINARY);
  CHECK(where.binaryOperator == BinaryOperator::AND);
}

TEST_CASE("UPDATE with WHERE OR") {
  auto result = parse("UPDATE users SET active = false WHERE role = 'guest' OR "
                      "role = 'banned';");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  CHECK(result.expressions[result.statement.whereIndex].binaryOperator ==
        BinaryOperator::OR);
}

TEST_CASE("UPDATE with WHERE IS NULL") {
  auto result =
      parse("UPDATE users SET active = false WHERE deleted_at IS NULL;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.binaryOperator == BinaryOperator::IS);
  CHECK(result.expressions[where.leftIndex].columnName == "deleted_at");
  CHECK(result.expressions[where.rightIndex].kind ==
        ExpressionKind::LITERAL_NULL);
}

TEST_CASE("UPDATE with WHERE IS NOT NULL") {
  auto result =
      parse("UPDATE users SET active = true WHERE email IS NOT NULL;");
  CHECK(result.error.empty());
  REQUIRE(result.statement.whereIndex != -1);
  auto &where = result.expressions[result.statement.whereIndex];
  CHECK(where.binaryOperator == BinaryOperator::IS_NOT);
  CHECK(result.expressions[where.leftIndex].columnName == "email");
  CHECK(result.expressions[where.rightIndex].kind ==
        ExpressionKind::LITERAL_NULL);
}

TEST_CASE("UPDATE missing table name produces error") {
  auto result = parse("UPDATE SET age = 1;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("UPDATE missing SET keyword produces error") {
  auto result = parse("UPDATE users age = 1;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("UPDATE missing equals in assignment produces error") {
  auto result = parse("UPDATE users SET age 30;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("UPDATE missing value in assignment produces error") {
  auto result = parse("UPDATE users SET age =;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("UPDATE incomplete WHERE clause produces error") {
  auto result = parse("UPDATE users SET age = 1 WHERE;");
  CHECK_FALSE(result.error.empty());
}

TEST_CASE("UPDATE WHERE with incomplete expression produces error") {
  auto result = parse("UPDATE users SET age = 1 WHERE id =;");
  CHECK_FALSE(result.error.empty());
}
