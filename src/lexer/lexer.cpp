#include "lexer.h"
#include "token.h"
#include <cctype>
#include <string>
#include <vector>

void Lexer::advance() { this->position++; }
char Lexer::peek() {
  if (this->position + 1 < query.size()) {
    return this->query[this->position + 1];
  }
  return '\0';
}
char Lexer::current() {
  if (this->position < query.size()) {
    return this->query[this->position];
  }
  return '\0';
}
std::vector<Token> Lexer::tokenize() {
  int currentLine = 1;
  std::vector<Token> tokens;
  while (this->current() != '\0') {
    char c = this->current();
    switch (c) {
    case ' ':
      break;
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
      // Comments start with -- until the end of the line
      if (this->peek() == '-') {
        while (this->current() != '\n') {
          this->advance();
        }
        break;
      }
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
          {.type = TokenType::DOT, .value = ".", .line = currentLine});
    } break;
    case '(': {
      tokens.push_back(
          {.type = TokenType::LEFT_PAREN, .value = "(", .line = currentLine});
    } break;
    case ')': {
      tokens.push_back(
          {.type = TokenType::RIGHT_PAREN, .value = ")", .line = currentLine});
    } break;
    case '\'': {
      int i = this->position + 1;
      this->advance();
      while (this->current() != '\'' && this->current() != '\0') {
        this->advance();
      }
      if (this->position == this->query.size()) {
        tokens.push_back({.type = TokenType::ERROR,
                          .value = "Unterminated string literal",
                          .line = currentLine});
        break;
      }
      std::string literal = query.substr(i, this->position - i);
      tokens.push_back({.type = TokenType::STRING_LITERAL,
                        .value = literal,
                        .line = currentLine});
    } break;
    default: {
      if (std::isalpha(current()) || current() == '_') {
        size_t i = this->position;
        while (std::isalnum(current()) || current() == '_') {
          advance();
        }

        std::string word = query.substr(i, this->position - i);
        tokens.push_back({getTokenTypeAlpha(word), word, currentLine});
        continue;
      }
      if (std::isdigit(current())) {
        size_t i = this->position;
        bool hasDot = false;

        while (std::isdigit(current()) || (!hasDot && current() == '.')) {
          if (current() == '.')
            hasDot = true;
          advance();
        }

        std::string num = query.substr(i, this->position - i);
        tokens.push_back({getTokenTypeDigit(num), num, currentLine});
        continue;
      }
    }
    }
    this->advance();
  }
  tokens.push_back(
      {.type = TokenType::END_OF_FILE, .value = "\0", .line = currentLine});
  return tokens;
}
