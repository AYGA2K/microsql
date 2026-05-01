#include "fmt/base.h"
#include "lexer/lexer.h"
#include "lexer/token.h"
#include <string>
int main() {
  std::string query = "Select * from users;";
  fmt::println("{}", query);
  Lexer lexer;
  lexer.query = query;
  auto tokens = lexer.tokenize();
  for (auto token : tokens) {
    fmt::println("{}", toString(token));
  }
  return 0;
}
