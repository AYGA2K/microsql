#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "catalog/catalog.h"
#include <doctest/doctest.h>
#include <fstream>

static ColumnDefinition makeCol(const std::string &name, DataType type,
                                int textLength = 0, bool notNull = false,
                                bool primaryKey = false) {
  ColumnDefinition col;
  col.name = name;
  col.type = type;
  col.textLength = textLength;
  col.notNull = notNull;
  col.primaryKey = primaryKey;
  return col;
}

static TableSchema makeTable(const std::string &name,
                             std::vector<ColumnDefinition> cols = {},
                             const std::string &filePath = "") {
  TableSchema schema;
  schema.tableName = name;
  schema.columns = std::move(cols);
  schema.filePath = filePath;
  return schema;
}

struct CatalogFile {
  CatalogFile() {}
  ~CatalogFile() { std::remove("catalog.txt"); }
  CatalogFile(const CatalogFile &) = delete;
  CatalogFile &operator=(const CatalogFile &) = delete;
};

TEST_CASE("load returns FileNotOpen when catalog.txt does not exist") {
  std::remove("catalog.txt");
  Catalog cat;
  auto result = cat.load();
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == CatalogError::FileNotOpen);
}

TEST_CASE("load returns UnvalidTableLineFormat for malformed TABLE line") {
  CatalogFile cleanup;
  std::ofstream f("catalog.txt");
  f << "TABLE\n";
  f.close();

  Catalog cat;
  auto result = cat.load();
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == CatalogError::UnvalidTableLineFormat);
}

TEST_CASE("load returns UnvalidColumnLineFormat for malformed COLUMN line") {
  CatalogFile cleanup;
  std::ofstream f("catalog.txt");
  f << "TABLE users\n";
  f << "COLUMN id INTEGER\n";
  f << "END\n";
  f.close();

  Catalog cat;
  auto result = cat.load();
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == CatalogError::UnvalidColumnLineFormat);
}

TEST_CASE("load parses a single table with all column types") {
  CatalogFile cleanup;
  std::ofstream f("catalog.txt");
  f << "TABLE products\n";
  f << "COLUMN id INTEGER 0 true true\n";
  f << "COLUMN price FLOAT 0 false false\n";
  f << "COLUMN name TEXT 32 true false\n";
  f << "COLUMN active BOOLEAN 0 false false\n";
  f << "FILE products.db\n";
  f << "END\n";
  f.close();

  Catalog cat;
  REQUIRE(cat.load().has_value());
  REQUIRE(cat.tables.size() == 1);

  const auto &t = cat.tables[0];
  CHECK(t.tableName == "products");
  CHECK(t.filePath == "products.db");
  REQUIRE(t.columns.size() == 4);

  CHECK(t.columns[0].name == "id");
  CHECK(t.columns[0].type == DataType::INTEGER);
  CHECK(t.columns[0].notNull == true);
  CHECK(t.columns[0].primaryKey == true);

  CHECK(t.columns[1].name == "price");
  CHECK(t.columns[1].type == DataType::FLOAT);

  CHECK(t.columns[2].name == "name");
  CHECK(t.columns[2].type == DataType::TEXT);
  CHECK(t.columns[2].textLength == 32);

  CHECK(t.columns[3].name == "active");
  CHECK(t.columns[3].type == DataType::BOOLEAN);
}

TEST_CASE("load parses multiple tables") {
  CatalogFile cleanup;
  std::ofstream f("catalog.txt");
  f << "TABLE users\n";
  f << "COLUMN id INTEGER 0 true true\n";
  f << "FILE users.db\n";
  f << "END\n";
  f << "TABLE orders\n";
  f << "COLUMN order_id INTEGER 0 true true\n";
  f << "FILE orders.db\n";
  f << "END\n";
  f.close();

  Catalog cat;
  REQUIRE(cat.load().has_value());
  CHECK(cat.tables.size() == 2);
  CHECK(cat.tables[0].tableName == "users");
  CHECK(cat.tables[1].tableName == "orders");
}

TEST_CASE(
    "save and load roundtrip preserves all fields across multiple tables") {
  CatalogFile cleanup;
  Catalog cat;
  cat.addTable(
      makeTable("employees",
                {makeCol("id", DataType::INTEGER, 0, true, true),
                 makeCol("name", DataType::TEXT, 50, true, false),
                 makeCol("salary", DataType::FLOAT, 0, false, false),
                 makeCol("active", DataType::BOOLEAN, 0, false, false)},
                "employees.db"));
  cat.addTable(
      makeTable("departments",
                {makeCol("dept_id", DataType::INTEGER, 0, true, true),
                 makeCol("dept_name", DataType::TEXT, 30, true, false)},
                "departments.db"));
  cat.addTable(
      makeTable("projects",
                {makeCol("proj_id", DataType::INTEGER, 0, true, true),
                 makeCol("budget", DataType::FLOAT, 0, false, false),
                 makeCol("archived", DataType::BOOLEAN, 0, false, false)},
                "projects.db"));
  cat.save();

  Catalog loaded;
  REQUIRE(loaded.load().has_value());
  REQUIRE(loaded.tables.size() == 3);

  const auto &emp = loaded.tables[0];
  CHECK(emp.tableName == "employees");
  CHECK(emp.filePath == "employees.db");
  REQUIRE(emp.columns.size() == 4);
  CHECK(emp.columns[0].name == "id");
  CHECK(emp.columns[0].type == DataType::INTEGER);
  CHECK(emp.columns[0].primaryKey == true);
  CHECK(emp.columns[1].name == "name");
  CHECK(emp.columns[1].type == DataType::TEXT);
  CHECK(emp.columns[1].textLength == 50);
  CHECK(emp.columns[2].type == DataType::FLOAT);
  CHECK(emp.columns[3].type == DataType::BOOLEAN);

  const auto &dept = loaded.tables[1];
  CHECK(dept.tableName == "departments");
  CHECK(dept.filePath == "departments.db");
  REQUIRE(dept.columns.size() == 2);
  CHECK(dept.columns[0].name == "dept_id");
  CHECK(dept.columns[0].primaryKey == true);
  CHECK(dept.columns[1].name == "dept_name");
  CHECK(dept.columns[1].textLength == 30);

  const auto &proj = loaded.tables[2];
  CHECK(proj.tableName == "projects");
  CHECK(proj.filePath == "projects.db");
  REQUIRE(proj.columns.size() == 3);
  CHECK(proj.columns[0].name == "proj_id");
  CHECK(proj.columns[1].type == DataType::FLOAT);
  CHECK(proj.columns[2].type == DataType::BOOLEAN);
}

TEST_CASE("addTable appends to tables vector") {
  Catalog cat;
  CHECK(cat.tables.empty());
  cat.addTable(makeTable("foo"));
  CHECK(cat.tables.size() == 1);
  cat.addTable(makeTable("bar"));
  CHECK(cat.tables.size() == 2);
}

TEST_CASE("dropTable removes the named table") {
  Catalog cat;
  cat.addTable(makeTable("alpha"));
  cat.addTable(makeTable("beta"));
  cat.addTable(makeTable("gamma"));

  cat.dropTable("beta");
  REQUIRE(cat.tables.size() == 2);
  CHECK(cat.tables[0].tableName == "alpha");
  CHECK(cat.tables[1].tableName == "gamma");
}

TEST_CASE("dropTable is a no-op for unknown name") {
  Catalog cat;
  cat.addTable(makeTable("only"));
  cat.dropTable("missing");
  CHECK(cat.tables.size() == 1);
}

TEST_CASE("findTable returns pointer to existing table") {
  Catalog cat;
  cat.addTable(makeTable("target"));
  auto *p = cat.findTable("target");
  REQUIRE(p != nullptr);
  CHECK(p->tableName == "target");
}

TEST_CASE("findTable returns nullptr for missing table") {
  Catalog cat;
  cat.addTable(makeTable("other"));
  CHECK(cat.findTable("ghost") == nullptr);
}

TEST_CASE("findTable returns nullptr on empty catalog") {
  Catalog cat;
  CHECK(cat.findTable("anything") == nullptr);
}
