#include "catalog/catalog.h"
#include "executor/executor.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include <cstdio>
#include <format>
#include <iostream>
#include <print>
#include <string>

static std::string formatValue(const Value &v) {
    return std::visit([](auto &&val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return "NULL";
        } else if constexpr (std::is_same_v<T, bool>) {
            return val ? "true" : "false";
        } else {
            return std::format("{}", val);
        }
    }, v);
}

static void printResult(const Result &result) {
    if (!result.success) {
        std::println("Error: {}", result.message);
        return;
    }
    if (result.columns.empty()) {
        if (!result.message.empty()) {
            std::println("{}", result.message);
        }
        return;
    }

    std::vector<size_t> widths(result.columns.size());
    for (size_t i = 0; i < result.columns.size(); ++i) {
        widths[i] = result.columns[i].size();
    }
    for (const auto &row : result.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], formatValue(row[i]).size());
        }
    }

    auto printSep = [&]() {
        std::print("+");
        for (size_t w : widths) {
            std::print("{:-<{}}+", "", w + 2);
        }
        std::println("");
    };

    printSep();
    std::print("|");
    for (size_t i = 0; i < result.columns.size(); ++i) {
        std::print(" {:<{}} |", result.columns[i], widths[i]);
    }
    std::println("");
    printSep();

    for (const auto &row : result.rows) {
        std::print("|");
        for (size_t i = 0; i < row.size(); ++i) {
            std::print(" {:<{}} |", formatValue(row[i]), widths[i]);
        }
        std::println("");
    }
    printSep();
    if (!result.message.empty()) {
        std::println("{}", result.message);
    }
}

int main() {
    ExecutionContext ctx;
    auto loaded = ctx.catalog.load();
    if (!loaded && loaded.error() != CatalogError::FileNotOpen) {
        std::println(stderr, "Warning: failed to load catalog (corrupted?)");
    }

    std::string buffer;
    std::string line;
    while (true) {
        if (buffer.empty()) {
            std::print(">>> ");
        } else {
            std::print("... ");
        }
        std::fflush(stdout);

        if (!std::getline(std::cin, line)) {
            break;
        }

        if (!buffer.empty()) {
            buffer += ' ';
        }
        buffer += line;

        auto semi = buffer.find(';');
        if (semi == std::string::npos) {
            continue;
        }

        std::string query = buffer.substr(0, semi + 1);
        buffer.clear();

        if (query == "exit;") {
            break;
        }

        Lexer lexer{query};
        ParseResult parsed = parse(lexer.tokenize());

        if (!parsed.error.empty()) {
            std::println("Parse error: {}", parsed.error);
            continue;
        }

        printResult(execute(parsed, ctx));
    }
}
