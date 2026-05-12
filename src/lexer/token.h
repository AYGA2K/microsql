#pragma once
#include <string>
enum class TokenType {
  SELECT,
  FROM,
  WHERE,
  INSERT,
  INTO,
  VALUES,
  UPDATE,
  SET,
  DELETE,
  CREATE,
  DROP,
  TABLE,
  INDEX,
  ON,
  BEGIN,
  COMMIT,
  ROLLBACK,
  AND,
  OR,
  NOT,
  IS,
  TKNULL,
  TRUE,
  FALSE,
  INTEGER,
  FLOAT,
  TEXT,
  BOOLEAN,
  IDENTIFIER,
  INTEGER_LITERAL,
  FLOAT_LITERAL,
  STRING_LITERAL,
  EQUAL,
  NOT_EQUAL,
  LESS_THAN,
  LESS_EQ,
  GREATER_THAN,
  GREATER_EQ,
  PLUS,
  MINUS,
  STAR,
  SLASH,
  COMMA,
  SEMICOLON,
  LEFT_PAREN,
  RIGHT_PAREN,
  DOT,
  END_OF_FILE,
  UNKNOWN,
  ERROR
};
struct Token {
  TokenType type;
  std::string value;
  int line;
};
std::string toString(const Token &token);

std::string toString(TokenType type);

TokenType getTokenTypeAlpha(std::string word);

TokenType getTokenTypeDigit(std::string word);
