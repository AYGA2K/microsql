#pragma once
#include "token.h"
#include <cstddef>
#include <vector>
struct Lexer {
  std::string query;
  size_t position = 0;
  void advance();
  char current();
  char peek();
  std::vector<Token> tokenize();
};
