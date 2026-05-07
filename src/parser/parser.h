#pragma once
#include "ast/expression.h"
#include "ast/statement.h"
#include "lexer/token.h"
#include <cstddef>
#include <vector>
struct ParseResult {
  Statement statement;
  std::vector<Expression> expressions;
};
ParseResult parse(const std::vector<Token> &tokens);

struct Parser {
  std::vector<Expression> expressions;
  const std::vector<Token> *tokens;
  size_t position;
  const Token &current();
  void advance();
};
