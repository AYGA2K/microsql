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
  TKNULL,
  TRUE,
  FALSE,
  INTEGER,
  FLOAT,
  TEXT,
  BOOLEAN
};
struct Token {
  TokenType type;
  std::string value;
  int line;
};
