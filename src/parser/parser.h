#pragma once
#include "ast/expression.h"
#include "ast/statement.h"
#include "lexer/token.h"
#include <cstddef>
#include <string>
#include <vector>
struct ParseResult {
  std::vector<Expression> expressions;
  Statement statement;
  std::string error;
};
ParseResult parse(const std::vector<Token> &tokens);
struct Parser {
  const std::vector<Token> *tokens;
  size_t position;
  ParseResult result;
  const Token &current();
  const Token &peek();
  void advance();
  void parseSelect();
  void parseInsert();
  void parseUpdate();
  void parseDelete();
  void parseCreate();
  void parseCreateTable();
  void parseCreateIndex();
  void parseDrop();
  bool parseExpression();
};
