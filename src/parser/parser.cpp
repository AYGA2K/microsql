#include "parser.h"
#include "ast/expression.h"
#include "ast/statement.h"
#include "lexer/token.h"
#include <optional>
#include <string>

const Token &Parser::current() {
  // Last token is always END_OF_FILE
  if (this->position >= this->tokens->size() - 1) {
    return this->tokens->back();
  }
  return (*this->tokens)[this->position];
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
  Statement statement;
  statement.kind = StatementKind::SELECT;
  if (this->current().type == TokenType::STAR) {
    this->advance();
  } else {
    while ((this->current().type != TokenType::FROM)) {
      if (this->current().type == TokenType::IDENTIFIER) {
        auto expression = this->parseExpression();
        if (expression) {
          statement.selectColumns.push_back(
              static_cast<int>(this->result.expressions.size()));
          this->result.expressions.push_back(expression.value());
        } else {
          this->result.error = "Unkown Expression at line " +
                               std::to_string(this->current().line);
          return;
        }
      }
      this->advance();
      if (this->current().type == TokenType::COMMA) {
        this->advance();
      }
    }
  }
  if (this->current().type == TokenType::FROM) {
    this->advance();
  } else {
    this->result.error =
        "Syntax error at line " + std::to_string(this->current().line) +
        ": expected FROM found '" + this->current().value + "'";
    return;
  }
  if (this->current().type == TokenType::IDENTIFIER) {
    statement.tableName = this->current().value;
  }
  this->advance();
  if (this->current().type == TokenType::WHERE) {
    // TODO: parse where
  } else {
    statement.whereIndex = -1;
  }
  this->result.statement = statement;
}

std::optional<Expression> Parser::parseExpression() {
  switch (this->current().type) {
  case TokenType::IDENTIFIER: {
    Expression expression;
    expression.columnName = this->current().value;
    expression.kind = ExpressionKind::COLUMN_REF;
    return expression;
  } break;
  default:
    return std::nullopt;
  }
}
