#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "storage/row.h"
#include <cstdint>
#include <cstring>
#include <doctest/doctest.h>
#include <vector>

static ColumnDefinition makeCol(DataType type, int textLength = 0) {
  ColumnDefinition col;
  col.type = type;
  col.textLength = textLength;
  return col;
}

TEST_CASE("serializeRow returns SchemaMismatch when row and schema sizes differ") {
  Schema schema = {makeCol(DataType::INTEGER)};
  Row row = {};
  auto result = serializeRow(row, schema);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == RowError::SchemaMismatch);
}

TEST_CASE("serializeRow INTEGER writes correct bytes") {
  Schema schema = {makeCol(DataType::INTEGER)};
  Row row = {int64_t(42)};
  auto result = serializeRow(row, schema);
  REQUIRE(result.has_value());
  int64_t out;
  std::memcpy(&out, result.value().data(), 8);
  CHECK(out == 42);
}

TEST_CASE("serializeRow BOOLEAN writes correct byte") {
  Schema schema = {makeCol(DataType::BOOLEAN)};
  Row row = {true};
  auto result = serializeRow(row, schema);
  REQUIRE(result.has_value());
  CHECK(result.value()[0] == 1);
}

TEST_CASE("serializeRow FLOAT writes correct bytes") {
  Schema schema = {makeCol(DataType::FLOAT)};
  Row row = {3.14};
  auto result = serializeRow(row, schema);
  REQUIRE(result.has_value());
  double out;
  std::memcpy(&out, result.value().data(), 8);
  CHECK(out == doctest::Approx(3.14));
}

TEST_CASE("serializeRow TEXT returns TextTooLong when string exceeds textLength") {
  Schema schema = {makeCol(DataType::TEXT, 5)};
  Row row = {std::string("toolongstring")};
  auto result = serializeRow(row, schema);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == RowError::TextTooLong);
}

TEST_CASE("serializeRow TEXT writes correct bytes") {
  Schema schema = {makeCol(DataType::TEXT, 5)};
  Row row = {std::string("hello")};
  auto result = serializeRow(row, schema);
  REQUIRE(result.has_value());
  uint16_t len;
  std::memcpy(&len, result.value().data(), 2);
  CHECK(len == 5);
  CHECK(std::memcmp(result.value().data() + 2, "hello", 5) == 0);
}

TEST_CASE("serializeRow multi-column row lays out fields contiguously") {
  Schema schema = {makeCol(DataType::INTEGER), makeCol(DataType::BOOLEAN)};
  Row row = {int64_t(7), false};
  auto result = serializeRow(row, schema);
  REQUIRE(result.has_value());
  int64_t ival;
  std::memcpy(&ival, result.value().data(), 8);
  CHECK(ival == 7);
  CHECK(result.value()[8] == 0);
}

TEST_CASE("deserializeRow INTEGER reads correct value") {
  Schema schema = {makeCol(DataType::INTEGER)};
  int64_t val = 123;
  std::vector<uint8_t> buf(8);
  std::memcpy(buf.data(), &val, 8);
  auto result = deserializeRow(buf.data(), schema);
  REQUIRE(result.has_value());
  CHECK(std::get<int64_t>((*result)[0]) == 123);
}

TEST_CASE("deserializeRow BOOLEAN reads correct value") {
  Schema schema = {makeCol(DataType::BOOLEAN)};
  std::vector<uint8_t> buf = {1};
  auto result = deserializeRow(buf.data(), schema);
  REQUIRE(result.has_value());
  CHECK(std::get<bool>((*result)[0]) == true);
}

TEST_CASE("deserializeRow FLOAT reads correct value") {
  Schema schema = {makeCol(DataType::FLOAT)};
  double val = 2.718;
  std::vector<uint8_t> buf(8);
  std::memcpy(buf.data(), &val, 8);
  auto result = deserializeRow(buf.data(), schema);
  REQUIRE(result.has_value());
  CHECK(std::get<double>((*result)[0]) == doctest::Approx(2.718));
}

TEST_CASE("deserializeRow TEXT reads correct value") {
  Schema schema = {makeCol(DataType::TEXT, 5)};
  std::vector<uint8_t> buf = {5, 0, 'h', 'e', 'l', 'l', 'o'};
  auto result = deserializeRow(buf.data(), schema);
  REQUIRE(result.has_value());
  CHECK(std::get<std::string>((*result)[0]) == "hello");
}

TEST_CASE("roundtrip INTEGER") {
  Schema schema = {makeCol(DataType::INTEGER)};
  Row original = {int64_t(-99)};
  auto serialized = serializeRow(original, schema);
  REQUIRE(serialized.has_value());
  auto result = deserializeRow(serialized.value().data(), schema);
  REQUIRE(result.has_value());
  CHECK(std::get<int64_t>((*result)[0]) == -99);
}

TEST_CASE("roundtrip BOOLEAN false") {
  Schema schema = {makeCol(DataType::BOOLEAN)};
  Row original = {false};
  auto serialized = serializeRow(original, schema);
  REQUIRE(serialized.has_value());
  auto result = deserializeRow(serialized.value().data(), schema);
  REQUIRE(result.has_value());
  CHECK(std::get<bool>((*result)[0]) == false);
}

TEST_CASE("roundtrip FLOAT") {
  Schema schema = {makeCol(DataType::FLOAT)};
  Row original = {1.5};
  auto serialized = serializeRow(original, schema);
  REQUIRE(serialized.has_value());
  auto result = deserializeRow(serialized.value().data(), schema);
  REQUIRE(result.has_value());
  CHECK(std::get<double>((*result)[0]) == doctest::Approx(1.5));
}

TEST_CASE("roundtrip TEXT") {
  Schema schema = {makeCol(DataType::TEXT, 4)};
  Row original = {std::string("test")};
  auto serialized = serializeRow(original, schema);
  REQUIRE(serialized.has_value());
  auto result = deserializeRow(serialized.value().data(), schema);
  REQUIRE(result.has_value());
  CHECK(std::get<std::string>((*result)[0]) == "test");
}

TEST_CASE("roundtrip mixed schema") {
  Schema schema = {
      makeCol(DataType::INTEGER),
      makeCol(DataType::FLOAT),
      makeCol(DataType::BOOLEAN),
      makeCol(DataType::TEXT, 4),
  };
  Row original = {int64_t(99), double(1.5), false, std::string("test")};
  auto serialized = serializeRow(original, schema);
  REQUIRE(serialized.has_value());
  auto result = deserializeRow(serialized.value().data(), schema);
  REQUIRE(result.has_value());
  const Row &row = *result;
  CHECK(std::get<int64_t>(row[0]) == 99);
  CHECK(std::get<double>(row[1]) == doctest::Approx(1.5));
  CHECK(std::get<bool>(row[2]) == false);
  CHECK(std::get<std::string>(row[3]) == "test");
}
