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

bool Parser::currentIs(TokenType type) { return this->current().type == type; }

bool Parser::peekIs(TokenType type) { return this->peek().type == type; }

bool Parser::consume(TokenType type, const std::string &name) {
  if (!this->currentIs(type)) {
    this->result.error = "Syntax error at line " +
                         std::to_string(this->current().line) + ": expected '" +
                         name + "' but found '" + this->current().value + "'";
    return false;
  }
  this->advance();
  return true;
}

bool Parser::consumeIdentifier(std::string &out, const std::string &context) {
  if (!this->currentIs(TokenType::IDENTIFIER)) {
    this->result.error = "Syntax error at line " +
                         std::to_string(this->current().line) + ": expected " +
                         context + " but found '" + this->current().value + "'";
    return false;
  }
  out = this->current().value;
  this->advance();
  return true;
}

int Parser::requireExpression(const std::string &context) {
  int index = this->parseExpression();
  if (index == -1 && this->result.error.empty()) {
    this->result.error = "Syntax error at line " +
                         std::to_string(this->current().line) + ": " + context;
  }
  return index;
}

bool Parser::parseAssignment() {
  std::string colName;
  if (!this->consumeIdentifier(colName, "column name"))
    return false;
  if (!this->consume(TokenType::EQUAL, "="))
    return false;
  int index = this->requireExpression("expected expression after '='");
  if (index == -1)
    return false;
  this->result.statement.assignments.push_back({colName, index});
  return true;
}

ParseResult parse(const std::vector<Token> &tokens) {
  Parser parser;
  parser.tokens = &tokens;
  parser.position = 0;

  const Token &token = parser.current();
  switch (token.type) {
  case TokenType::SELECT:
    parser.parseSelect();
    break;
  case TokenType::INSERT:
    parser.parseInsert();
    break;
  case TokenType::CREATE:
    parser.parseCreate();
    break;
  case TokenType::UPDATE:
    parser.parseUpdate();
    break;
  case TokenType::DELETE:
    parser.parseDelete();
    break;
  default:
    parser.result.error = "Syntax error at line " + std::to_string(token.line) +
                          ": unexpected token '" + token.value + "'";
  }

  return parser.result;
}

void Parser::parseSelect() {
  this->advance();
  this->result.statement.kind = StatementKind::SELECT;
  if (this->currentIs(TokenType::STAR)) {
    this->advance();
    if (!this->currentIs(TokenType::FROM) &&
        !this->currentIs(TokenType::SEMICOLON) &&
        !this->currentIs(TokenType::END_OF_FILE)) {
      this->result.error = "Syntax error at line " +
                           std::to_string(this->current().line) +
                           ": unexpected token '" + this->current().value + "'";
      return;
    }
  } else {
    while (!this->currentIs(TokenType::FROM) &&
           !this->currentIs(TokenType::SEMICOLON) &&
           !this->currentIs(TokenType::END_OF_FILE)) {
      int index = this->parseExpression();
      if (index == -1) {
        if (this->result.error.empty()) {
          this->result.error = "Unknown expression at line " +
                               std::to_string(this->current().line);
        }
        return;
      }
      this->result.statement.selectColumns.push_back(index);
      if (this->currentIs(TokenType::COMMA)) {
        this->advance();
      }
    }
  }
  if (this->currentIs(TokenType::FROM)) {
    this->advance();
    if (!this->consumeIdentifier(this->result.statement.tableName,
                                 "table name after FROM"))
      return;
    if (this->currentIs(TokenType::WHERE)) {
      this->advance();
      int index = this->requireExpression("expected expression after WHERE");
      if (index == -1)
        return;
      this->result.statement.whereIndex = index;
    } else {
      this->result.statement.whereIndex = -1;
    }
  }
}

// Precedence (lowest to highest): OR → AND → NOT → comparison → +/- → */÷ →
// unary → primary
int Parser::parseExpression() {
  int left = this->parseAnd();
  if (left == -1) {
    return -1;
  }
  while (this->currentIs(TokenType::OR)) {
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
  while (this->currentIs(TokenType::AND)) {
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
  while (this->currentIs(TokenType::NOT)) {
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
  while (this->currentIs(TokenType::EQUAL) ||
         this->currentIs(TokenType::NOT_EQUAL) ||
         this->currentIs(TokenType::LESS_THAN) ||
         this->currentIs(TokenType::LESS_EQ) ||
         this->currentIs(TokenType::GREATER_THAN) ||
         this->currentIs(TokenType::GREATER_EQ) ||
         this->currentIs(TokenType::IS)) {

    Expression expression;
    expression.kind = ExpressionKind::BINARY;
    std::string operatorValue = this->current().value;
    int operatorLine = this->current().line;
    if (this->currentIs(TokenType::EQUAL)) {
      expression.binaryOperator = BinaryOperator::EQUAL;
    } else if (this->currentIs(TokenType::NOT_EQUAL)) {
      expression.binaryOperator = BinaryOperator::NOT_EQUAL;
    } else if (this->currentIs(TokenType::LESS_THAN)) {
      expression.binaryOperator = BinaryOperator::LESS_THAN;
    } else if (this->currentIs(TokenType::LESS_EQ)) {
      expression.binaryOperator = BinaryOperator::LESS_THAN_OR_EQUAL;
    } else if (this->currentIs(TokenType::GREATER_THAN)) {
      expression.binaryOperator = BinaryOperator::GREATER_THAN;
    } else if (this->currentIs(TokenType::GREATER_EQ)) {
      expression.binaryOperator = BinaryOperator::GREATER_THAN_OR_EQUAL;
    } else if (this->currentIs(TokenType::IS)) {
      if (this->peekIs(TokenType::NOT)) {
        expression.binaryOperator = BinaryOperator::IS_NOT;
        operatorValue = "IS NOT";
        this->advance();
      } else {
        expression.binaryOperator = BinaryOperator::IS;
      }
    }
    this->advance();
    int right = parseAddSubstract();
    if (right == -1) {
      if (this->result.error.empty()) {
        this->result.error =
            "Syntax error at line " + std::to_string(operatorLine) +
            ": expected expression after '" + operatorValue + "'";
      }
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
  while (this->currentIs(TokenType::PLUS) ||
         this->currentIs(TokenType::MINUS)) {
    Expression expression;
    expression.kind = ExpressionKind::BINARY;
    if (this->currentIs(TokenType::PLUS)) {
      expression.binaryOperator = BinaryOperator::ADD;
    } else if (this->currentIs(TokenType::MINUS)) {
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
  while (this->currentIs(TokenType::STAR) ||
         this->currentIs(TokenType::SLASH)) {
    Expression expression;
    expression.kind = ExpressionKind::BINARY;
    if (this->currentIs(TokenType::STAR)) {
      expression.binaryOperator = BinaryOperator::MULTIPLY;
    } else if (this->currentIs(TokenType::SLASH)) {
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
  if (this->currentIs(TokenType::MINUS)) {
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
    if (this->peekIs(TokenType::DOT)) {
      expression.tablePrefix = this->current().value;
      this->advance();
      if (this->peekIs(TokenType::IDENTIFIER)) {
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
    expression.boolValue = this->currentIs(TokenType::TRUE);
    int index = static_cast<int>(this->result.expressions.size());
    this->result.expressions.push_back(expression);
    this->advance();
    return index;
  }
  case TokenType::LEFT_PAREN: {
    this->advance();
    int index = this->parseExpression();
    if (index == -1) {
      return -1;
    }
    if (!this->currentIs(TokenType::RIGHT_PAREN)) {
      this->result.error =
          "Syntax error at line " + std::to_string(this->current().line) +
          ": expected ')' but found '" + this->current().value + "'";
      return -1;
    }
    this->advance();
    return index;
  }
  default:
    return -1;
  }
}

void Parser::parseInsert() {
  this->advance();
  this->result.statement.kind = StatementKind::INSERT;
  if (!this->consume(TokenType::INTO, "INTO")) {
    return;
  }
  if (!this->consumeIdentifier(this->result.statement.tableName,
                               "table name")) {
    return;
  }
  if (this->currentIs(TokenType::LEFT_PAREN)) {
    this->advance();
    this->result.statement.insertColumnNames.push_back(this->current().value);
    this->advance();
    while (!this->currentIs(TokenType::RIGHT_PAREN) &&
           !this->currentIs(TokenType::SEMICOLON) &&
           !this->currentIs(TokenType::END_OF_FILE)) {
      if (this->currentIs(TokenType::COMMA)) {
        this->advance();
      }
      this->result.statement.insertColumnNames.push_back(this->current().value);
      this->advance();
      if (this->currentIs(TokenType::COMMA)) {
        this->advance();
      }
    }
    if (!this->consume(TokenType::RIGHT_PAREN, ")")) {
      return;
    }
  }
  if (!this->consume(TokenType::VALUES, "VALUES")) {
    return;
  }
  if (this->currentIs(TokenType::LEFT_PAREN)) {
    this->advance();
    this->result.statement.insertValues.push_back(parseExpression());
    if (this->currentIs(TokenType::COMMA)) {
      this->advance();
      while (!this->currentIs(TokenType::RIGHT_PAREN) &&
             !this->currentIs(TokenType::SEMICOLON) &&
             !this->currentIs(TokenType::END_OF_FILE)) {
        if (this->currentIs(TokenType::COMMA)) {
          this->advance();
        }
        this->result.statement.insertValues.push_back(parseExpression());
        if (this->currentIs(TokenType::COMMA)) {
          this->advance();
        }
      }
    }
    if (!this->consume(TokenType::RIGHT_PAREN, ")")) {
      return;
    }
    if (!this->consume(TokenType::SEMICOLON, ";")) {
      return;
    }
  }
}

void Parser::parseDelete() {
  this->advance();
  this->result.statement.kind = StatementKind::DELETE;
  if (!this->consume(TokenType::FROM, "FROM")) {
    return;
  }
  if (!this->consumeIdentifier(this->result.statement.tableName,
                               "table name")) {
    return;
  }
  if (this->currentIs(TokenType::WHERE)) {
    this->advance();
    int index = this->requireExpression("expected expression after WHERE");
    if (index == -1) {
      return;
    }
    this->result.statement.whereIndex = index;
  } else {
    this->result.statement.whereIndex = -1;
  }
  if (!this->consume(TokenType::SEMICOLON, ";"))
    return;
}

void Parser::parseUpdate() {
  this->advance();
  this->result.statement.kind = StatementKind::UPDATE;
  if (!this->consumeIdentifier(this->result.statement.tableName,
                               "table name")) {
    return;
  }

  if (!this->consume(TokenType::SET, "SET")) {
    return;
  }

  if (!this->parseAssignment()) {
    return;
  }

  while (this->currentIs(TokenType::COMMA)) {
    this->advance();
    if (!this->parseAssignment()) {
      return;
    }
  }

  if (this->currentIs(TokenType::WHERE)) {
    this->advance();
    int index = this->requireExpression("expected expression after WHERE");
    if (index == -1) {
      return;
    }
    this->result.statement.whereIndex = index;
  } else {
    this->result.statement.whereIndex = -1;
  }
  if (!this->consume(TokenType::SEMICOLON, ";")) {
    return;
  }
}

void Parser::parseCreate() {
  this->advance();
  const Token &token = this->current();
  if (token.value == "TABLE") {
    return parseCreateTable();
  }
  if (token.value == "INDEX") {
    return parseCreateIndex();
  }
  this->result.error =
      "Syntax error at line " + std::to_string(this->current().line) +
      ": expected 'TABLE' or 'INDEX' but found '" + this->current().value + "'";
  return;
}
void Parser::parseCreateTable() {
  this->advance();
  this->result.statement.kind = StatementKind::CREATE_TABLE;
  if (!this->consumeIdentifier(this->result.statement.tableName,
                               "table name")) {
    return;
  }
  if (!this->consume(TokenType::LEFT_PAREN, "(")) {
    return;
  }

  while (!this->currentIs(TokenType::RIGHT_PAREN) &&
         !this->currentIs(TokenType::SEMICOLON) &&
         !this->currentIs(TokenType::END_OF_FILE)) {
    ColumnDefinition colDefinition;
    if (!this->consumeIdentifier(colDefinition.name, "column name")) {
      return;
    }

    switch (this->current().type) {
    case TokenType::INTEGER:
      colDefinition.type = DataType::INTEGER;
      this->advance();
      break;
    case TokenType::FLOAT:
      colDefinition.type = DataType::FLOAT;
      this->advance();
      break;
    case TokenType::TEXT:
      colDefinition.type = DataType::TEXT;
      // Optional text length
      if (this->peekIs(TokenType::LEFT_PAREN)) {
        this->advance(); // skip type
        this->advance(); // skip (
        colDefinition.textLength = std::stoi(this->current().value);
        this->advance();
        if (!this->consume(TokenType::RIGHT_PAREN, ")")) {
          return;
        }
      } else {
        colDefinition.textLength = 255;
        this->advance();
      }
      break;
    case TokenType::BOOLEAN:
      colDefinition.type = DataType::BOOLEAN;
      this->advance();
      break;
    default:
      this->result.error =
          "Syntax error at line " + std::to_string(this->current().line) +
          ": expected 'INTEGER','FLOAT','TEXT' or 'BOOLEAN' but found '" +
          this->current().value + "'";
      return;
    }

    // Order of constraints does not matter
    // so we can start with PRIMARY key
    // or NOT NULL
    while (this->currentIs(TokenType::PRIMARY) ||
           this->currentIs(TokenType::NOT)) {
      if (this->currentIs(TokenType::PRIMARY) && this->peekIs(TokenType::KEY)) {
        colDefinition.primaryKey = true;
        this->advance(); // skip PRIMARY
        this->advance(); // skip KEY
      }
      if (this->currentIs(TokenType::NOT) && this->peekIs(TokenType::TKNULL)) {
        colDefinition.notNull = true;
        this->advance(); // skip NOT
        this->advance(); // skip NULL
      }
    }
    result.statement.columnDefinitions.push_back(colDefinition);
    if (this->currentIs(TokenType::COMMA)) {
      this->advance();
    }
  }
  this->advance();
  if (!this->consume(TokenType::SEMICOLON, ";")) {
    return;
  }
}
void Parser::parseCreateIndex() {
  this->advance();
  this->result.statement.kind = StatementKind::CREATE_INDEX;

  if (!this->consume(TokenType::ON, "ON")) {
    return;
  }

  if (!this->consumeIdentifier(this->result.statement.tableName,
                               "table name")) {
    return;
  }

  if (!this->consume(TokenType::LEFT_PAREN, "(")) {
    return;
  }

  if (!this->consumeIdentifier(this->result.statement.indexColumnName,
                               "Column name")) {
    return;
  }

  if (!this->consume(TokenType::RIGHT_PAREN, ")")) {
    return;
  }

  if (!this->consume(TokenType::SEMICOLON, ";")) {
    return;
  }
}
