#include "parser.h"
#include "lexer/token.h"

const Token &Parser::current() {
  // Last token is always END_OF_FILE
  if (this->position >= this->tokens->size() - 1) {
    return this->tokens->back();
  }
  return (*this->tokens)[this->position];
}
void Parser::advance() { this->position++; }
