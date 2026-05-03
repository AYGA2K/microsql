#include "lexer/token.h"
#include <iostream>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "lexer/lexer.h"
#include <doctest/doctest.h>

static std::vector<Token> lex(const std::string &query) {
  return Lexer{query}.tokenize();
}

TEST_CASE("SELECT *") {
  auto tokens = lex("SELECT * FROM users;");
  REQUIRE(tokens.size() == 6);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "users");
  CHECK(tokens[4].type == TokenType::SEMICOLON);
  CHECK(tokens[5].type == TokenType::END_OF_FILE);
}

TEST_CASE("SELECT with WHERE and integer") {
  auto tokens = lex("SELECT name FROM users WHERE age = 30;");
  REQUIRE(tokens.size() == 10);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::IDENTIFIER);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[6].type == TokenType::EQUAL);
  CHECK(tokens[7].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[7].value == "30");
  CHECK(tokens[8].type == TokenType::SEMICOLON);
  CHECK(tokens[9].type == TokenType::END_OF_FILE);
}

TEST_CASE("SELECT with string literal") {
  auto tokens = lex("SELECT * FROM users WHERE name = 'alice';");
  REQUIRE(tokens.size() == 10);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "users");
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].value == "name");
  CHECK(tokens[6].type == TokenType::EQUAL);
  CHECK(tokens[7].type == TokenType::STRING_LITERAL);
  CHECK(tokens[7].value == "alice");
  CHECK(tokens[8].type == TokenType::SEMICOLON);
  CHECK(tokens[9].type == TokenType::END_OF_FILE);
}

TEST_CASE("SELECT with float literal") {
  auto tokens = lex("SELECT * FROM products WHERE price > 9.99;");
  REQUIRE(tokens.size() == 10);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "products");
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].value == "price");
  CHECK(tokens[6].type == TokenType::GREATER_THAN);
  CHECK(tokens[7].type == TokenType::FLOAT_LITERAL);
  CHECK(tokens[7].value == "9.99");
  CHECK(tokens[8].type == TokenType::SEMICOLON);
  CHECK(tokens[9].type == TokenType::END_OF_FILE);
}

TEST_CASE("INSERT INTO") {
  auto tokens = lex("INSERT INTO users VALUES (1, 'alice');");
  REQUIRE(tokens.size() == 11);
  CHECK(tokens[0].type == TokenType::INSERT);
  CHECK(tokens[1].type == TokenType::INTO);
  CHECK(tokens[2].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].type == TokenType::VALUES);
  CHECK(tokens[4].type == TokenType::LEFT_PAREN);
  CHECK(tokens[5].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[5].value == "1");
  CHECK(tokens[6].type == TokenType::COMMA);
  CHECK(tokens[7].type == TokenType::STRING_LITERAL);
  CHECK(tokens[7].value == "alice");
  CHECK(tokens[8].type == TokenType::RIGHT_PAREN);
  CHECK(tokens[9].type == TokenType::SEMICOLON);
  CHECK(tokens[10].type == TokenType::END_OF_FILE);
}

TEST_CASE("UPDATE SET") {
  auto tokens = lex("UPDATE users SET age = 31 WHERE id = 1;");
  REQUIRE(tokens.size() == 12);
  CHECK(tokens[0].type == TokenType::UPDATE);
  CHECK(tokens[1].type == TokenType::IDENTIFIER);
  CHECK(tokens[2].type == TokenType::SET);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[4].type == TokenType::EQUAL);
  CHECK(tokens[5].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[6].type == TokenType::WHERE);
  CHECK(tokens[7].type == TokenType::IDENTIFIER);
  CHECK(tokens[8].type == TokenType::EQUAL);
  CHECK(tokens[9].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[10].type == TokenType::SEMICOLON);
  CHECK(tokens[11].type == TokenType::END_OF_FILE);
}

TEST_CASE("DELETE") {
  auto tokens = lex("DELETE FROM users WHERE id = 5;");
  REQUIRE(tokens.size() == 9);
  CHECK(tokens[0].type == TokenType::DELETE);
  CHECK(tokens[1].type == TokenType::FROM);
  CHECK(tokens[2].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].type == TokenType::WHERE);
  CHECK(tokens[4].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].type == TokenType::EQUAL);
  CHECK(tokens[6].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[7].type == TokenType::SEMICOLON);
  CHECK(tokens[8].type == TokenType::END_OF_FILE);
}

TEST_CASE("CREATE TABLE") {
  auto tokens = lex("CREATE TABLE users (id INTEGER, name TEXT);");
  REQUIRE(tokens.size() == 12);
  CHECK(tokens[0].type == TokenType::CREATE);
  CHECK(tokens[1].type == TokenType::TABLE);
  CHECK(tokens[2].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].type == TokenType::LEFT_PAREN);
  CHECK(tokens[4].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].type == TokenType::INTEGER);
  CHECK(tokens[6].type == TokenType::COMMA);
  CHECK(tokens[7].type == TokenType::IDENTIFIER);
  CHECK(tokens[8].type == TokenType::TEXT);
  CHECK(tokens[9].type == TokenType::RIGHT_PAREN);
  CHECK(tokens[10].type == TokenType::SEMICOLON);
  CHECK(tokens[11].type == TokenType::END_OF_FILE);
}

TEST_CASE("DROP TABLE") {
  auto tokens = lex("DROP TABLE users;");
  REQUIRE(tokens.size() == 5);
  CHECK(tokens[0].type == TokenType::DROP);
  CHECK(tokens[1].type == TokenType::TABLE);
  CHECK(tokens[2].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].type == TokenType::SEMICOLON);
  CHECK(tokens[4].type == TokenType::END_OF_FILE);
}

TEST_CASE("transaction block") {
  auto tokens = lex("BEGIN; COMMIT;");
  REQUIRE(tokens.size() == 5);
  CHECK(tokens[0].type == TokenType::BEGIN);
  CHECK(tokens[1].type == TokenType::SEMICOLON);
  CHECK(tokens[2].type == TokenType::COMMIT);
  CHECK(tokens[3].type == TokenType::SEMICOLON);
  CHECK(tokens[4].type == TokenType::END_OF_FILE);
}

TEST_CASE("WHERE with AND / OR") {
  auto tokens = lex("SELECT * FROM t WHERE a = 1 AND b = 2 OR c = 3;");
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[6].type == TokenType::EQUAL);
  CHECK(tokens[7].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[8].type == TokenType::AND);
  CHECK(tokens[9].type == TokenType::IDENTIFIER);
  CHECK(tokens[10].type == TokenType::EQUAL);
  CHECK(tokens[11].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[12].type == TokenType::OR);
  CHECK(tokens[13].type == TokenType::IDENTIFIER);
  CHECK(tokens[14].type == TokenType::EQUAL);
  CHECK(tokens[15].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[16].type == TokenType::SEMICOLON);
  CHECK(tokens[17].type == TokenType::END_OF_FILE);
}

TEST_CASE("WHERE with comparison operators") {
  auto tokens =
      lex("SELECT * FROM t WHERE a != 1 AND b <= 2 AND c >= 3 AND d <> 4;");
  REQUIRE(tokens.size() == 22);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "t");
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].value == "a");
  CHECK(tokens[6].type == TokenType::NOT_EQUAL);
  CHECK(tokens[7].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[7].value == "1");
  CHECK(tokens[8].type == TokenType::AND);
  CHECK(tokens[9].type == TokenType::IDENTIFIER);
  CHECK(tokens[9].value == "b");
  CHECK(tokens[10].type == TokenType::LESS_EQ);
  CHECK(tokens[11].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[11].value == "2");
  CHECK(tokens[12].type == TokenType::AND);
  CHECK(tokens[13].type == TokenType::IDENTIFIER);
  CHECK(tokens[13].value == "c");
  CHECK(tokens[14].type == TokenType::GREATER_EQ);
  CHECK(tokens[15].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[15].value == "3");
  CHECK(tokens[16].type == TokenType::AND);
  CHECK(tokens[17].type == TokenType::IDENTIFIER);
  CHECK(tokens[17].value == "d");
  CHECK(tokens[18].type == TokenType::NOT_EQUAL);
  CHECK(tokens[19].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[19].value == "4");
  CHECK(tokens[20].type == TokenType::SEMICOLON);
  CHECK(tokens[21].type == TokenType::END_OF_FILE);
}

TEST_CASE("keywords are case-insensitive") {
  auto tokens = lex("select * from users;");
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[2].type == TokenType::FROM);
}

TEST_CASE("inline comment is ignored") {
  auto tokens = lex("SELECT * -- pick all\nFROM users;");
  REQUIRE(tokens.size() == 6);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
}

TEST_CASE("line numbers are tracked") {
  auto tokens = lex("SELECT *\nFROM users\nWHERE id = 1;");
  CHECK(tokens[0].line == 1);
  CHECK(tokens[2].line == 2);
  CHECK(tokens[4].line == 3);
}

TEST_CASE("dotted identifier (table.column)") {
  auto tokens = lex("SELECT u.name FROM u;");

  for (auto token : tokens) {
    std::cout << toString(token) << std::endl;
  }
  REQUIRE(tokens.size() == 7);
  CHECK(tokens[1].type == TokenType::IDENTIFIER);
  CHECK(tokens[2].type == TokenType::DOT);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
}

TEST_CASE("empty input") {
  auto tokens = lex("");
  REQUIRE(tokens.size() == 1);
  CHECK(tokens[0].type == TokenType::END_OF_FILE);
}

TEST_CASE("ROLLBACK transaction") {
  auto tokens = lex("BEGIN; ROLLBACK;");
  REQUIRE(tokens.size() == 5);
  CHECK(tokens[0].type == TokenType::BEGIN);
  CHECK(tokens[0].value == "BEGIN");
  CHECK(tokens[1].type == TokenType::SEMICOLON);
  CHECK(tokens[2].type == TokenType::ROLLBACK);
  CHECK(tokens[2].value == "ROLLBACK");
  CHECK(tokens[3].type == TokenType::SEMICOLON);
  CHECK(tokens[4].type == TokenType::END_OF_FILE);
}

TEST_CASE("CREATE INDEX ON") {
  auto tokens = lex("CREATE INDEX myidx ON users (age);");
  REQUIRE(tokens.size() == 10);
  CHECK(tokens[0].type == TokenType::CREATE);
  CHECK(tokens[1].type == TokenType::INDEX);
  CHECK(tokens[1].value == "INDEX");
  CHECK(tokens[2].type == TokenType::IDENTIFIER);
  CHECK(tokens[2].value == "myidx");
  CHECK(tokens[3].type == TokenType::ON);
  CHECK(tokens[3].value == "ON");
  CHECK(tokens[4].type == TokenType::IDENTIFIER);
  CHECK(tokens[4].value == "users");
  CHECK(tokens[5].type == TokenType::LEFT_PAREN);
  CHECK(tokens[6].type == TokenType::IDENTIFIER);
  CHECK(tokens[6].value == "age");
  CHECK(tokens[7].type == TokenType::RIGHT_PAREN);
  CHECK(tokens[8].type == TokenType::SEMICOLON);
  CHECK(tokens[9].type == TokenType::END_OF_FILE);
}

TEST_CASE("NULL literal") {
  auto tokens = lex("SELECT name FROM users WHERE age = NULL;");
  REQUIRE(tokens.size() == 10);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::IDENTIFIER);
  CHECK(tokens[1].value == "name");
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "users");
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].value == "age");
  CHECK(tokens[6].type == TokenType::EQUAL);
  CHECK(tokens[7].type == TokenType::TKNULL);
  CHECK(tokens[7].value == "NULL");
  CHECK(tokens[8].type == TokenType::SEMICOLON);
  CHECK(tokens[9].type == TokenType::END_OF_FILE);
}

TEST_CASE("TRUE and FALSE literals") {
  auto tokens = lex("SELECT * FROM t WHERE a = TRUE AND b = FALSE;");
  REQUIRE(tokens.size() == 14);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "t");
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].value == "a");
  CHECK(tokens[6].type == TokenType::EQUAL);
  CHECK(tokens[7].type == TokenType::TRUE);
  CHECK(tokens[7].value == "TRUE");
  CHECK(tokens[8].type == TokenType::AND);
  CHECK(tokens[9].type == TokenType::IDENTIFIER);
  CHECK(tokens[9].value == "b");
  CHECK(tokens[10].type == TokenType::EQUAL);
  CHECK(tokens[11].type == TokenType::FALSE);
  CHECK(tokens[11].value == "FALSE");
  CHECK(tokens[12].type == TokenType::SEMICOLON);
  CHECK(tokens[13].type == TokenType::END_OF_FILE);
}

TEST_CASE("FLOAT and BOOLEAN column types in CREATE TABLE") {
  auto tokens = lex("CREATE TABLE t (x FLOAT, y BOOLEAN);");
  REQUIRE(tokens.size() == 12);
  CHECK(tokens[0].type == TokenType::CREATE);
  CHECK(tokens[1].type == TokenType::TABLE);
  CHECK(tokens[2].type == TokenType::IDENTIFIER);
  CHECK(tokens[2].value == "t");
  CHECK(tokens[3].type == TokenType::LEFT_PAREN);
  CHECK(tokens[4].type == TokenType::IDENTIFIER);
  CHECK(tokens[4].value == "x");
  CHECK(tokens[5].type == TokenType::FLOAT);
  CHECK(tokens[5].value == "FLOAT");
  CHECK(tokens[6].type == TokenType::COMMA);
  CHECK(tokens[7].type == TokenType::IDENTIFIER);
  CHECK(tokens[7].value == "y");
  CHECK(tokens[8].type == TokenType::BOOLEAN);
  CHECK(tokens[8].value == "BOOLEAN");
  CHECK(tokens[9].type == TokenType::RIGHT_PAREN);
  CHECK(tokens[10].type == TokenType::SEMICOLON);
  CHECK(tokens[11].type == TokenType::END_OF_FILE);
}

TEST_CASE("LESS_THAN operator") {
  auto tokens = lex("SELECT * FROM t WHERE age < 18;");
  REQUIRE(tokens.size() == 10);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "t");
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].value == "age");
  CHECK(tokens[6].type == TokenType::LESS_THAN);
  CHECK(tokens[6].value == "<");
  CHECK(tokens[7].type == TokenType::INTEGER_LITERAL);
  CHECK(tokens[7].value == "18");
  CHECK(tokens[8].type == TokenType::SEMICOLON);
  CHECK(tokens[9].type == TokenType::END_OF_FILE);
}

TEST_CASE("arithmetic operators PLUS MINUS SLASH") {
  auto tokens = lex("SELECT a + b - c / d FROM t;");
  REQUIRE(tokens.size() == 12);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::IDENTIFIER);
  CHECK(tokens[1].value == "a");
  CHECK(tokens[2].type == TokenType::PLUS);
  CHECK(tokens[2].value == "+");
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "b");
  CHECK(tokens[4].type == TokenType::MINUS);
  CHECK(tokens[4].value == "-");
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].value == "c");
  CHECK(tokens[6].type == TokenType::SLASH);
  CHECK(tokens[6].value == "/");
  CHECK(tokens[7].type == TokenType::IDENTIFIER);
  CHECK(tokens[7].value == "d");
  CHECK(tokens[8].type == TokenType::FROM);
  CHECK(tokens[9].type == TokenType::IDENTIFIER);
  CHECK(tokens[9].value == "t");
  CHECK(tokens[10].type == TokenType::SEMICOLON);
  CHECK(tokens[11].type == TokenType::END_OF_FILE);
}

TEST_CASE("UNKNOWN token for unrecognized character") {
  auto tokens = lex("SELECT * FROM t WHERE a ! b;");
  REQUIRE(tokens.size() == 10);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "t");
  CHECK(tokens[4].type == TokenType::WHERE);
  CHECK(tokens[5].type == TokenType::IDENTIFIER);
  CHECK(tokens[5].value == "a");
  CHECK(tokens[6].type == TokenType::UNKNOWN);
  CHECK(tokens[6].value == "!");
  CHECK(tokens[7].type == TokenType::IDENTIFIER);
  CHECK(tokens[7].value == "b");
  CHECK(tokens[8].type == TokenType::SEMICOLON);
  CHECK(tokens[9].type == TokenType::END_OF_FILE);
}

TEST_CASE("unterminated string literal produces ERROR") {
  auto tokens = lex("SELECT 'alice FROM t;");
  REQUIRE(tokens.size() == 3);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::ERROR);
  CHECK(tokens[1].value == "Unterminated string literal");
  CHECK(tokens[2].type == TokenType::END_OF_FILE);
}

TEST_CASE("tab is treated as whitespace") {
  auto tokens = lex("SELECT\t*\tFROM\tusers;");
  REQUIRE(tokens.size() == 6);
  CHECK(tokens[0].type == TokenType::SELECT);
  CHECK(tokens[1].type == TokenType::STAR);
  CHECK(tokens[2].type == TokenType::FROM);
  CHECK(tokens[3].type == TokenType::IDENTIFIER);
  CHECK(tokens[3].value == "users");
  CHECK(tokens[4].type == TokenType::SEMICOLON);
  CHECK(tokens[5].type == TokenType::END_OF_FILE);
}
