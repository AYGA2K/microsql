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
  CHECK(tokens[6].type == TokenType::EQUAL);
  CHECK(tokens[7].type == TokenType::STRING_LITERAL);
  CHECK(tokens[7].value == "alice");
}

TEST_CASE("SELECT with float literal") {
  auto tokens = lex("SELECT * FROM products WHERE price > 9.99;");
  REQUIRE(tokens.size() == 10);
  CHECK(tokens[6].type == TokenType::GREATER_THAN);
  CHECK(tokens[7].type == TokenType::FLOAT_LITERAL);
  CHECK(tokens[7].value == "9.99");
}

TEST_CASE("INSERT INTO") {
  auto tokens = lex("INSERT INTO users VALUES (1, 'alice');");
  REQUIRE(tokens.size() == 10);
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
}

TEST_CASE("DROP TABLE") {
  auto tokens = lex("DROP TABLE users;");
  REQUIRE(tokens.size() == 4);
  CHECK(tokens[0].type == TokenType::DROP);
  CHECK(tokens[1].type == TokenType::TABLE);
  CHECK(tokens[2].type == TokenType::IDENTIFIER);
}

TEST_CASE("transaction block") {
  auto tokens = lex("BEGIN; COMMIT;");
  REQUIRE(tokens.size() == 5);
  CHECK(tokens[0].type == TokenType::BEGIN);
  CHECK(tokens[1].type == TokenType::SEMICOLON);
  CHECK(tokens[2].type == TokenType::COMMIT);
  CHECK(tokens[3].type == TokenType::SEMICOLON);
}

TEST_CASE("WHERE with AND / OR") {
  auto tokens = lex("SELECT * FROM t WHERE a = 1 AND b = 2 OR c = 3;");
  CHECK(tokens[7].type == TokenType::AND);
  CHECK(tokens[11].type == TokenType::OR);
}

TEST_CASE("WHERE with comparison operators") {
  auto tokens = lex("SELECT * FROM t WHERE a != 1 AND b <= 2 AND c >= 3 AND d <> 4;");
  CHECK(tokens[6].type == TokenType::NOT_EQUAL);
  CHECK(tokens[10].type == TokenType::LESS_EQ);
  CHECK(tokens[14].type == TokenType::GREATER_EQ);
  CHECK(tokens[18].type == TokenType::NOT_EQUAL);
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
