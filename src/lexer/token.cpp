#include "token.h"

std::string toUpper(std::string s) {
  for (char &c : s) {
    c = std::toupper(static_cast<unsigned char>(c));
  }
  return s;
}
bool isKeyword(std::string word) {
  word = toUpper(word);
  if (word == "SELECT" || word == "WHERE") {
    return true;
  }
  return false;
}
