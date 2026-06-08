#include "lexer/lexer.h"
#include "lexer/token.h"
#include <print>
#include <string>
int main() {
  std::string query = "Select * from users;";
  std::println("{}", query);
  Lexer lexer;
  lexer.query = query;
  auto tokens = lexer.tokenize();
  for (auto token : tokens) {
    std::println("{}", toString(token));
  }
  return 0;
}
