#pragma once
#include "token.h"
#include <vector>
struct Lexer {
  std::string query;
  int pos;
  void advance();
  char current();
  char peek();
  std::vector<Token> tokenize();
};
