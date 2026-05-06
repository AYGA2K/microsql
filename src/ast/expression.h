#pragma once
#include <cstdint>
#include <string>
enum class ExpressionKind {
  LITERAL_INT,
  LITERAL_FLOAT,
  LITERAL_TEXT,
  LITERAL_BOOL,
  LITERAL_NULL,
  COLUMN_REF,
  BINARY,
  UNARY,
  STAR,
};

enum class BinaryOperator {
  EQUAL,
  NOT_EQUAL,
  LESS_THAN,
  LESS_THAN_OR_EQUAL,
  GREATER_THAN,
  GREATER_THAN_OR_EQUAL,
  AND,
  OR,
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
};

enum class UnaryOperator {
  NOT,
  NEGATE,
};

struct Expression {
  ExpressionKind kind;

  int64_t intValue;

  double floatValue;

  std::string textValue;

  bool boolValue;

  std::string columnName;
  std::string tablePrefix; // "users" for users.age; empty for plain age

  BinaryOperator binaryOperator;
  int leftIndex; // index into ParseResult.expressions
  int rightIndex;

  UnaryOperator unaryOperator;
  int operandIndex;
};
