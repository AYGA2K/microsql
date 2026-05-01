#include "token.h"
#include <unordered_map>

std::string toUpper(std::string s) {
  for (char &c : s) {
    c = std::toupper(static_cast<unsigned char>(c));
  }
  return s;
}
TokenType getTokenType(std::string word) {
  static const std::unordered_map<std::string, TokenType> keywordMap = {
      {"SELECT", TokenType::SELECT},
      {"FROM", TokenType::FROM},
      {"WHERE", TokenType::WHERE},
      {"INSERT", TokenType::INSERT},
      {"INTO", TokenType::INTO},
      {"VALUES", TokenType::VALUES},
      {"UPDATE", TokenType::UPDATE},
      {"SET", TokenType::SET},
      {"DELETE", TokenType::DELETE},
      {"CREATE", TokenType::CREATE},
      {"DROP", TokenType::DROP},
      {"TABLE", TokenType::TABLE},
      {"INDEX", TokenType::INDEX},
      {"ON", TokenType::ON},
      {"BEGIN", TokenType::BEGIN},
      {"COMMIT", TokenType::COMMIT},
      {"ROLLBACK", TokenType::ROLLBACK},
      {"AND", TokenType::AND},
      {"OR", TokenType::OR},
      {"NULL", TokenType::TKNULL},
      {"TRUE", TokenType::TRUE},
      {"FALSE", TokenType::FALSE},
      {"INTEGER", TokenType::INTEGER},
      {"FLOAT", TokenType::FLOAT},
      {"TEXT", TokenType::TEXT},
      {"BOOLEAN", TokenType::BOOLEAN}};
  auto it = keywordMap.find(word);
  if (it != keywordMap.end()) {
    return it->second;
  }
  return TokenType::IDENTIFIER;
}

std::string toString(TokenType type) {
  switch (type) {
  case TokenType::SELECT:
    return "SELECT";
  case TokenType::FROM:
    return "FROM";
  case TokenType::WHERE:
    return "WHERE";
  case TokenType::INSERT:
    return "INSERT";
  case TokenType::INTO:
    return "INTO";
  case TokenType::VALUES:
    return "VALUES";
  case TokenType::UPDATE:
    return "UPDATE";
  case TokenType::SET:
    return "SET";
  case TokenType::DELETE:
    return "DELETE";
  case TokenType::CREATE:
    return "CREATE";
  case TokenType::DROP:
    return "DROP";
  case TokenType::TABLE:
    return "TABLE";
  case TokenType::INDEX:
    return "INDEX";
  case TokenType::ON:
    return "ON";
  case TokenType::BEGIN:
    return "BEGIN";
  case TokenType::COMMIT:
    return "COMMIT";
  case TokenType::ROLLBACK:
    return "ROLLBACK";
  case TokenType::AND:
    return "AND";
  case TokenType::OR:
    return "OR";
  case TokenType::TKNULL:
    return "TKNULL";
  case TokenType::TRUE:
    return "TRUE";
  case TokenType::FALSE:
    return "FALSE";
  case TokenType::INTEGER:
    return "INTEGER";
  case TokenType::FLOAT:
    return "FLOAT";
  case TokenType::TEXT:
    return "TEXT";
  case TokenType::BOOLEAN:
    return "BOOLEAN";
  case TokenType::IDENTIFIER:
    return "IDENTIFIER";
  case TokenType::INTEGER_LITERAL:
    return "INTEGER_LITERAL";
  case TokenType::FLOAT_LITERAL:
    return "FLOAT_LITERAL";
  case TokenType::STRING_LITERAL:
    return "STRING_LITERAL";
  case TokenType::EQUAL:
    return "EQUAL";
  case TokenType::NOT_EQUAL:
    return "NOT_EQUAL";
  case TokenType::LESS_THAN:
    return "LESS_THAN";
  case TokenType::LESS_EQ:
    return "LESS_EQ";
  case TokenType::GREATER_THAN:
    return "GREATER_THAN";
  case TokenType::GREATER_EQ:
    return "GREATER_EQ";
  case TokenType::PLUS:
    return "PLUS";
  case TokenType::MINUS:
    return "MINUS";
  case TokenType::STAR:
    return "STAR";
  case TokenType::SLASH:
    return "SLASH";
  case TokenType::COMMA:
    return "COMMA";
  case TokenType::SEMICOLON:
    return "SEMICOLON";
  case TokenType::LEFT_PAREN:
    return "LEFT_PAREN";
  case TokenType::RIGHT_PAREN:
    return "RIGHT_PAREN";
  case TokenType::DOT:
    return "DOT";
  case TokenType::END_OF_FILE:
    return "END_OF_FILE";
  case TokenType::UNKNOWN:
    return "UNKNOWN";
  default:
    return "UNKNOWN";
  }
}

std::string toString(const Token &token) {
  return "Token{ type=" + toString(token.type) + ", value=\"" + token.value +
         "\", line=" + std::to_string(token.line) + " }";
}
