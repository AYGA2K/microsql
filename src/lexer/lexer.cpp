#include "lexer.h"
#include "token.h"
#include <cctype>
#include <string>
#include <vector>

void Lexer::advance() { this->pos++; }
char Lexer::peek() {
  if (pos + 1 < query.size()) {
    return this->query[pos + 1];
  }
  return '\0';
}
char Lexer::current() {
  if (this->pos < this->query.size()) {
    return this->query[pos];
  }
  return '\0';
}
std::vector<Token> Lexer::tokenize() {
  int currentLine = 1;
  std::vector<Token> tokens;
  while (this->current() != '\0') {
    char c = this->current();
    switch (c) {
    case '\n': {
      currentLine++;
    } break;
    case '=': {
      tokens.push_back(
          {.type = TokenType::EQUAL, .value = "=", .line = currentLine});
    } break;
    case '!': {
      if (this->peek() == '=') {
        tokens.push_back(
            {.type = TokenType::NOT_EQUAL, .value = "!=", .line = currentLine});
        this->advance();
        break;
      }
      tokens.push_back(
          {.type = TokenType::UNKNOWN, .value = "!", .line = currentLine});
    } break;
    case '<': {
      if (this->peek() == '=') {
        tokens.push_back(
            {.type = TokenType::LESS_EQ, .value = "<=", .line = currentLine});
        this->advance();
        break;
      }
      if (this->peek() == '>') {
        tokens.push_back(
            {.type = TokenType::NOT_EQUAL, .value = "<>", .line = currentLine});
        this->advance();
        break;
      }
      tokens.push_back(
          {.type = TokenType::LESS_THAN, .value = "<", .line = currentLine});
    } break;
    case '>': {
      if (this->peek() == '=') {
        tokens.push_back({.type = TokenType::GREATER_EQ,
                          .value = ">=",
                          .line = currentLine});
        this->advance();
        break;
      }
      tokens.push_back(
          {.type = TokenType::GREATER_THAN, .value = ">", .line = currentLine});
    } break;
    case '+': {
      tokens.push_back(
          {.type = TokenType::PLUS, .value = "+", .line = currentLine});
    } break;
    case '-': {
      tokens.push_back(
          {.type = TokenType::MINUS, .value = "-", .line = currentLine});
    } break;
    case '*': {
      tokens.push_back(
          {.type = TokenType::STAR, .value = "*", .line = currentLine});
    } break;
    case '/': {
      tokens.push_back(
          {.type = TokenType::SLASH, .value = "/", .line = currentLine});
    } break;
    case ',': {
      tokens.push_back(
          {.type = TokenType::COMMA, .value = ",", .line = currentLine});
    } break;
    case ';': {
      tokens.push_back(
          {.type = TokenType::SEMICOLON, .value = ";", .line = currentLine});
    } break;
    case '.': {
      tokens.push_back(
          {.type = TokenType::DOT, .value = ";", .line = currentLine});
    } break;
    case '(': {
      tokens.push_back(
          {.type = TokenType::LEFT_PAREN, .value = ";", .line = currentLine});
    } break;
    case ')': {
      tokens.push_back(
          {.type = TokenType::RIGHT_PAREN, .value = ";", .line = currentLine});
    } break;
    }
    if (std::isalnum(this->current())) {
      int i = this->pos;
      while (std::isalnum(this->current())) {
        this->advance();
      }

      std::string word = query.substr(this->pos, this->pos - i);
      word = toUpper(word);
      tokens.push_back(
          {.type = getTokenType(word), .value = word, .line = currentLine});
      continue;
    }
    this->advance();
  }
  tokens.push_back({
      .type = TokenType::END_OF_FILE,
      .value = "\0",
  });
  return tokens;
}
