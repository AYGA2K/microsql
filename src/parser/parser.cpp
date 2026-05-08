#include "parser.h"
#include "ast/statement.h"
#include "lexer/token.h"
#include <string>

const Token &Parser::current() {
  // Last token is always END_OF_FILE
  if (this->position >= this->tokens->size() - 1) {
    return this->tokens->back();
  }
  return (*this->tokens)[this->position];
}

void Parser::advance() { this->position++; }

ParseResult parse(const std::vector<Token> &tokens) {
  Parser parser;
  parser.tokens = &tokens;
  parser.position = 0;

  const Token &token = parser.current();
  switch (token.type) {
  case TokenType::SELECT:
    parser.parseSelect();
    break;
  case TokenType::INSERT:
    parser.parseInsert();
    break;
  case TokenType::CREATE:
    parser.parseCreate();
    break;
  case TokenType::UPDATE:
    parser.parseUpdate();
    break;
  case TokenType::DELETE:
    parser.parseDelete();
    break;
  default:
    parser.result.error = "Syntax error at line " + std::to_string(token.line) +
                          ": unexpected token '" + token.value + "'";
  }

  return parser.result;
}

void Parser::parseSelect() {
  this->advance();
  Statement statement;
  statement.kind = StatementKind::SELECT;
  if (this->current().type == TokenType::STAR) {
    this->advance();
  } else {
    // TODO: parse select columns
  }
  if (this->current().type == TokenType::FROM) {
    this->advance();
  } else {
    this->result.error =
        "Syntax error at line " + std::to_string(this->current().line) +
        ": expected FROM found '" + this->current().value + "'";
    return;
  }
  statement.tableName = this->current().value;
  this->advance();
  this->result.statement = statement;
}
