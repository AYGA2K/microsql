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
  } else {
    while ((this->current().type != TokenType::FROM) &&
           (this->current().type != TokenType::SEMICOLON)) {
      bool ok = this->parseExpression();
      if (!ok) {
        if (this->result.error.empty()) {
          this->result.error = "Unkown Expression at line " +
                               std::to_string(this->current().line);
        }
        return;
      }
      this->advance();
      if (this->current().type == TokenType::COMMA) {
        this->advance();
      }
    }
  }
  if (this->current().type == TokenType::FROM) {
    this->advance();
    if (this->current().type == TokenType::IDENTIFIER) {
      this->result.statement.tableName = this->current().value;
      this->advance();
      if (this->current().type == TokenType::WHERE) {
        // TODO: parse where
      } else {
        this->result.statement.whereIndex = -1;
      }
    }
  }
}

bool Parser::parseExpression() {
  Expression expression;
  switch (this->current().type) {
  case TokenType::IDENTIFIER: {
    const auto &peekToken = this->peek();
    if ((peekToken.type != TokenType::END_OF_FILE) &&
        peekToken.type == TokenType::DOT) {
      expression.tablePrefix = this->current().value;
      expression.kind = ExpressionKind::COLUMN_REF;
      this->advance();
      if (this->peek().type == TokenType::IDENTIFIER) {
        this->advance();
      } else {
        this->result.error = "Expreceted table name found " +
                             this->peek().value + " at line " +
                             std::to_string(this->current().line);
        return false;
      }
    }
    expression.columnName = this->current().value;
    expression.kind = ExpressionKind::COLUMN_REF;
    this->result.statement.selectColumns.push_back(
        static_cast<int>(this->result.expressions.size()));
    this->result.expressions.push_back(expression);
    return true;
  } break;
  case TokenType::INTEGER_LITERAL: {
    expression.kind = ExpressionKind::LITERAL_INT;
    expression.intValue = std::stoi(this->current().value);
    this->result.statement.selectColumns.push_back(
        static_cast<int>(this->result.expressions.size()));
    this->result.expressions.push_back(expression);
    return true;
  } break;

  case TokenType::FLOAT_LITERAL: {
    expression.kind = ExpressionKind::LITERAL_FLOAT;
    expression.floatValue = std::stof(this->current().value);
    this->result.statement.selectColumns.push_back(
        static_cast<int>(this->result.expressions.size()));
    this->result.expressions.push_back(expression);
    return true;
  } break;

  case TokenType::STRING_LITERAL: {
    expression.kind = ExpressionKind::LITERAL_TEXT;
    expression.textValue = this->current().value;
    this->result.statement.selectColumns.push_back(
        static_cast<int>(this->result.expressions.size()));
    this->result.expressions.push_back(expression);
    return true;
  } break;
  default:
    return false;
  }
}
