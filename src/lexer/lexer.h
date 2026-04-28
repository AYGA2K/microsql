#pragma once
#include "token.h"
#include <vector>
struct Lexer {
  std::vector<Token> tokenize();
};
