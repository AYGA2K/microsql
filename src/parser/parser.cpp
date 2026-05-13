#include "parser.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "lexer/token.h"
#include <string>

const Token &Parser::current() {
  // Last token is always END_OF_FILE
  if (this->position >= this->tokens->size() - 1) {
    return this->tokens->back();
  }
  return (*this->tokens)[this->position];
}

const Token &Parser::peek() {
  // Last token is always END_OF_FILE
  if (this->position + 1 >= this->tokens->size() - 1) {
    return this->tokens->back();
  }
  return (*this->tokens)[this->position + 1];
}

void Parser::advance() { this->position++; }

ParseResult parse(const std::vector<Token> &tokens) {
  Parser parser;
  parser.tokens = &tokens;
  parser.position = 0;

  const Token &token = parser.current();
  switch (token.type) {
  case TokenType::SELECT:
    parser.parseSelect();
    break;
  // case TokenType::INSERT:
  //   parser.parseInsert();
  //   break;
  // case TokenType::CREATE:
  //   parser.parseCreate();
  //   break;
  // case TokenType::UPDATE:
  //   parser.parseUpdate();
  //   break;
  // case TokenType::DELETE:
  //   parser.parseDelete();
  //   break;
  default:
    parser.result.error = "Syntax error at line " + std::to_string(token.line) +
                          ": unexpected token '" + token.value + "'";
  }

  return parser.result;
}

void Parser::parseSelect() {
  this->advance();
  this->result.statement.kind = StatementKind::SELECT;
  if (this->current().type == TokenType::STAR) {
    this->advance();
    if (this->current().type != TokenType::FROM &&
        this->current().type != TokenType::SEMICOLON &&
        this->current().type != TokenType::END_OF_FILE) {
      this->result.error = "Syntax error at line " +
                           std::to_string(this->current().line) +
                           ": unexpected token '" + this->current().value + "'";
      return;
    }
  } else {
    while ((this->current().type != TokenType::FROM) &&
           (this->current().type != TokenType::SEMICOLON)) {
      int index = this->parseExpression();
      if (index == -1) {
        if (this->result.error.empty()) {
          this->result.error = "Unknown expression at line " +
                               std::to_string(this->current().line);
        }
        return;
      }
      this->result.statement.selectColumns.push_back(index);
      if (this->current().type == TokenType::COMMA) {
        this->advance();
      }
    }
  }
  if (this->current().type == TokenType::FROM) {
    this->advance();
    if (this->current().type != TokenType::IDENTIFIER) {
      this->result.error = "Syntax error at line " +
                           std::to_string(this->current().line) +
                           ": expected table name after FROM but found '" +
                           this->current().value + "'";
      return;
    }
    this->result.statement.tableName = this->current().value;
    this->advance();
    if (this->current().type == TokenType::WHERE) {
      this->advance();
      int index = this->parseExpression();
      if (index == -1) {
        if (this->result.error.empty()) {
          this->result.error = "Syntax error at line " +
                               std::to_string(this->current().line) +
                               ": expected expression after WHERE";
        }
        return;
      }
      this->result.statement.whereIndex = index;
    } else {
      this->result.statement.whereIndex = -1;
    }
  }
}

int Parser::parseExpression() {
  int left = this->parseAnd();
  if (left == -1) {
    return -1;
  }
  while (this->current().type == TokenType::OR) {
    this->advance();
    int right = this->parseAnd();
    if (right == -1) {
      return -1;
    }
    Expression expression;
    expression.kind = ExpressionKind::BINARY;
    expression.binaryOperator = BinaryOperator::OR;
    expression.leftIndex = left;
    expression.rightIndex = right;
    left = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
  }
  return left;
}

int Parser::parseAnd() {
  int left = this->parseNot();
  if (left == -1) {
    return -1;
  }
  while (this->current().type == TokenType::AND) {
    this->advance();
    int right = this->parseNot();
    if (right == -1) {
      return -1;
    }
    Expression expression;
    expression.kind = ExpressionKind::BINARY;
    expression.binaryOperator = BinaryOperator::AND;
    expression.leftIndex = left;
    expression.rightIndex = right;
    left = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
  }
  return left;
}

int Parser::parseNot() {
  if (this->current().type == TokenType::NOT) {
    this->advance();
    int operandIndex = parseComparison();
    if (operandIndex == -1) {
      return -1;
    }
    Expression expression;
    expression.kind = ExpressionKind::UNARY;
    expression.unaryOperator = UnaryOperator::NOT;
    expression.operandIndex = operandIndex;
    int index = this->result.expressions.size();
    this->result.expressions.push_back(expression);
    return index;
  }
  return this->parseComparison();
}

int Parser::parseComparison() {
  int left = this->parseAddSubstract();
  if (left == -1) {
    return -1;
  }
  while (this->current().type == TokenType::EQUAL ||
         this->current().type == TokenType::NOT_EQUAL ||
         this->current().type == TokenType::LESS_THAN ||
         this->current().type == TokenType::LESS_EQ ||
         this->current().type == TokenType::GREATER_THAN ||
         this->current().type == TokenType::GREATER_EQ ||
         this->current().type == TokenType::IS) {

    Expression expression;
    expression.kind = ExpressionKind::BINARY;
    if (this->current().type == TokenType::EQUAL) {
      expression.binaryOperator = BinaryOperator::EQUAL;
    } else if (this->current().type == TokenType::NOT_EQUAL) {
      expression.binaryOperator = BinaryOperator::NOT_EQUAL;
    } else if (this->current().type == TokenType::LESS_THAN) {
      expression.binaryOperator = BinaryOperator::LESS_THAN;
    } else if (this->current().type == TokenType::LESS_EQ) {
      expression.binaryOperator = BinaryOperator::LESS_THAN_OR_EQUAL;
    } else if (this->current().type == TokenType::GREATER_THAN) {
      expression.binaryOperator = BinaryOperator::GREATER_THAN;
    } else if (this->current().type == TokenType::GREATER_EQ) {
      expression.binaryOperator = BinaryOperator::GREATER_THAN_OR_EQUAL;
    } else if (this->current().type == TokenType::IS) {
      if (this->peek().type == TokenType::NOT) {
        expression.binaryOperator = BinaryOperator::IS_NOT;
        this->advance();
      } else {
        expression.binaryOperator = BinaryOperator::IS;
      }
    }
    this->advance();
    int right = parseAddSubstract();
    if (right == -1) {
      return -1;
    }
    expression.leftIndex = left;
    expression.rightIndex = right;
    left = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
  }
  return left;
}

int Parser::parseAddSubstract() {
  int left = this->parseMultiplyDivide();
  if (left == -1) {
    return -1;
  }
  while (this->current().type == TokenType::PLUS ||
         this->current().type == TokenType::MINUS) {
    Expression expression;
    expression.kind = ExpressionKind::BINARY;
    if (this->current().type == TokenType::PLUS) {
      expression.binaryOperator = BinaryOperator::ADD;
    } else if (this->current().type == TokenType::MINUS) {
      expression.binaryOperator = BinaryOperator::SUBTRACT;
    }
    this->advance();
    int right = this->parseMultiplyDivide();
    if (right == -1) {
      return -1;
    }
    expression.leftIndex = left;
    expression.rightIndex = right;
    left = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
  }
  return left;
}

int Parser::parseMultiplyDivide() {
  int left = parseUnary();
  if (left == -1) {
    return -1;
  }
  while (this->current().type == TokenType::STAR ||
         this->current().type == TokenType::SLASH) {
    Expression expression;
    expression.kind = ExpressionKind::BINARY;
    if (this->current().type == TokenType::STAR) {
      expression.binaryOperator = BinaryOperator::MULTIPLY;
    } else if (this->current().type == TokenType::SLASH) {
      expression.binaryOperator = BinaryOperator::DIVIDE;
    }
    this->advance();
    int right = this->parseUnary();
    if (right == -1) {
      return -1;
    }
    expression.leftIndex = left;
    expression.rightIndex = right;
    left = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
  }
  return left;
}

int Parser::parseUnary() {
  if (this->current().type == TokenType::MINUS) {
    this->advance();
    int operandIndex = parsePrimary();
    if (operandIndex == -1) {
      return -1;
    }
    Expression expression;
    expression.kind = ExpressionKind::UNARY;
    expression.unaryOperator = UnaryOperator::NEGATE;
    expression.operandIndex = operandIndex;
    int index = this->result.expressions.size();
    this->result.expressions.push_back(expression);
    return index;
  }
  return this->parsePrimary();
}

int Parser::parsePrimary() {
  Expression expression;
  switch (this->current().type) {
  case TokenType::IDENTIFIER: {
    if (this->peek().type == TokenType::DOT) {
      expression.tablePrefix = this->current().value;
      this->advance();
      if (this->peek().type == TokenType::IDENTIFIER) {
        this->advance();
      } else {
        this->result.error = "Expected column name after '.' but found '" +
                             this->peek().value + "' at line " +
                             std::to_string(this->current().line);
        return -1;
      }
    }
    expression.columnName = this->current().value;
    expression.kind = ExpressionKind::COLUMN_REF;
    int index = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
    this->advance();
    return index;
  }
  case TokenType::INTEGER_LITERAL: {
    expression.kind = ExpressionKind::LITERAL_INT;
    expression.intValue = std::stoi(this->current().value);
    int index = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
    this->advance();
    return index;
  }
  case TokenType::FLOAT_LITERAL: {
    expression.kind = ExpressionKind::LITERAL_FLOAT;
    expression.floatValue = std::stof(this->current().value);
    int index = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
    this->advance();
    return index;
  }
  case TokenType::STRING_LITERAL: {
    expression.kind = ExpressionKind::LITERAL_TEXT;
    expression.textValue = this->current().value;
    int index = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
    this->advance();
    return index;
  }
  case TokenType::TKNULL: {
    expression.kind = ExpressionKind::LITERAL_NULL;
    expression.textValue = this->current().value;
    int index = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
    this->advance();
    return index;
  }
  case TokenType::TRUE:
  case TokenType::FALSE: {
    expression.kind = ExpressionKind::LITERAL_BOOL;
    expression.boolValue = this->current().type == TokenType::TRUE;
    int index = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
    this->advance();
    return index;
  }
  default:
    return -1;
  }
}
