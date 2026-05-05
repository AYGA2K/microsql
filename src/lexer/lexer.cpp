#include "lexer.h"
#include "token.h"
#include <cctype>
#include <string>
#include <vector>

void Lexer::advance() { this->pos++; }
char Lexer::peek() {
  if (pos + 1 < static_cast<int>(query.size())) {
    return this->query[pos + 1];
  }
  return '\0';
}
char Lexer::current() {
  if (this->pos < static_cast<int>(query.size())) {
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
      int i = this->pos + 1;
      this->advance();
      while (this->current() != '\'' && this->current() != '\0') {
        this->advance();
      }
      if (this->pos == static_cast<int>(this->query.size())) {
        tokens.push_back({.type = TokenType::ERROR,
                          .value = "Unterminated string literal",
                          .line = currentLine});
        break;
      }
      std::string literal = query.substr(i, this->pos - i);
      tokens.push_back({.type = TokenType::STRING_LITERAL,
                        .value = literal,
                        .line = currentLine});
    } break;
    default: {
      if (std::isalpha(current()) || current() == '_') {
        int i = pos;
        while (std::isalnum(current()) || current() == '_') {
          advance();
        }

        std::string word = query.substr(i, pos - i);
        tokens.push_back({getTokenTypeAlpha(word), word, currentLine});
        continue;
      }
      if (std::isdigit(current())) {
        int i = pos;
        bool hasDot = false;

        while (std::isdigit(current()) || (!hasDot && current() == '.')) {
          if (current() == '.')
            hasDot = true;
          advance();
        }

        std::string num = query.substr(i, pos - i);
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
