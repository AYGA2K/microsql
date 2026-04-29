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
