#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "executor/executor.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include <doctest/doctest.h>
#include <cstdio>
#include <string>
#include <vector>

static Result run(const std::string &sql, ExecutionContext &ctx) {
    Lexer lexer{sql};
    ParseResult parsed = parse(lexer.tokenize());
    if (!parsed.error.empty()) {
        Result r;
        r.success = false;
        r.message = parsed.error;
        return r;
    }
    return execute(parsed, ctx);
}

struct Cleanup {
    std::vector<std::string> files;
    ~Cleanup() {
        for (const auto &f : files) {
            std::remove(f.c_str());
        }
    }
};

TEST_CASE("create table then insert then select") {
    Cleanup c{{"catalog.txt", "users.ms"}};
    ExecutionContext ctx;

    auto cr = run("CREATE TABLE users (id INTEGER, name TEXT(50));", ctx);
    REQUIRE(cr.success);

    REQUIRE(run("INSERT INTO users VALUES (1, 'alice');", ctx).success);
    REQUIRE(run("INSERT INTO users VALUES (2, 'bob');", ctx).success);

    auto sel = run("SELECT * FROM users;", ctx);
    REQUIRE(sel.success);
    REQUIRE(sel.rows.size() == 2);
    CHECK(std::get<int64_t>(sel.rows[0][0]) == 1);
    CHECK(std::get<std::string>(sel.rows[0][1]) == "alice");
    CHECK(std::get<int64_t>(sel.rows[1][0]) == 2);
    CHECK(std::get<std::string>(sel.rows[1][1]) == "bob");
}

TEST_CASE("select with where") {
    Cleanup c{{"catalog.txt", "items.ms"}};
    ExecutionContext ctx;

    REQUIRE(run("CREATE TABLE items (id INTEGER, val INTEGER);", ctx).success);
    REQUIRE(run("INSERT INTO items VALUES (1, 10);", ctx).success);
    REQUIRE(run("INSERT INTO items VALUES (2, 20);", ctx).success);
    REQUIRE(run("INSERT INTO items VALUES (3, 30);", ctx).success);

    auto sel = run("SELECT * FROM items WHERE id = 2;", ctx);
    REQUIRE(sel.success);
    REQUIRE(sel.rows.size() == 1);
    CHECK(std::get<int64_t>(sel.rows[0][0]) == 2);
    CHECK(std::get<int64_t>(sel.rows[0][1]) == 20);
}

TEST_CASE("update with where then select") {
    Cleanup c{{"catalog.txt", "scores.ms"}};
    ExecutionContext ctx;

    REQUIRE(run("CREATE TABLE scores (id INTEGER, score INTEGER);", ctx).success);
    REQUIRE(run("INSERT INTO scores VALUES (1, 100);", ctx).success);
    REQUIRE(run("INSERT INTO scores VALUES (2, 200);", ctx).success);

    auto upd = run("UPDATE scores SET score = 999 WHERE id = 1;", ctx);
    REQUIRE(upd.success);
    CHECK(upd.message == "1 row updated");

    auto sel = run("SELECT * FROM scores;", ctx);
    REQUIRE(sel.success);
    REQUIRE(sel.rows.size() == 2);
    CHECK(std::get<int64_t>(sel.rows[0][1]) == 999);
    CHECK(std::get<int64_t>(sel.rows[1][1]) == 200);
}

TEST_CASE("delete with where then select") {
    Cleanup c{{"catalog.txt", "logs.ms"}};
    ExecutionContext ctx;

    REQUIRE(run("CREATE TABLE logs (id INTEGER, val INTEGER);", ctx).success);
    REQUIRE(run("INSERT INTO logs VALUES (1, 10);", ctx).success);
    REQUIRE(run("INSERT INTO logs VALUES (2, 20);", ctx).success);
    REQUIRE(run("INSERT INTO logs VALUES (3, 30);", ctx).success);

    auto del = run("DELETE FROM logs WHERE id = 2;", ctx);
    REQUIRE(del.success);
    CHECK(del.message == "1 row deleted");

    auto sel = run("SELECT * FROM logs;", ctx);
    REQUIRE(sel.success);
    REQUIRE(sel.rows.size() == 2);
    CHECK(std::get<int64_t>(sel.rows[0][0]) == 1);
    CHECK(std::get<int64_t>(sel.rows[1][0]) == 3);
}


TEST_CASE("data persists across execution context reload") {
    Cleanup c{{"catalog.txt", "persist.ms"}};

    {
        ExecutionContext ctx;
        REQUIRE(run("CREATE TABLE persist (id INTEGER, val INTEGER);", ctx).success);
        REQUIRE(run("INSERT INTO persist VALUES (42, 99);", ctx).success);
    }

    {
        ExecutionContext ctx;
        REQUIRE(ctx.catalog.load().has_value());
        auto sel = run("SELECT * FROM persist;", ctx);
        REQUIRE(sel.success);
        REQUIRE(sel.rows.size() == 1);
        CHECK(std::get<int64_t>(sel.rows[0][0]) == 42);
        CHECK(std::get<int64_t>(sel.rows[0][1]) == 99);
    }
}

TEST_CASE("parse error on bad sql") {
    ExecutionContext ctx;
    auto r = run("SELEKT * FROM foo;", ctx);
    REQUIRE_FALSE(r.success);
}

TEST_CASE("insert into unknown table fails") {
    ExecutionContext ctx;
    auto r = run("INSERT INTO ghost VALUES (1);", ctx);
    REQUIRE_FALSE(r.success);
}
