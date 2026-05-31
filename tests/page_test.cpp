#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "page/page.h"
#include <doctest/doctest.h>
#include <cstring>

static Page makePage(uint32_t id = 1) {
  Page p;
  p.init(id);
  return p;
}

static std::vector<uint8_t> makeRow(uint16_t len, uint8_t fill = 0xAB) {
  return std::vector<uint8_t>(len, fill);
}

TEST_CASE("init sets clean state") {
  Page p = makePage(42);
  CHECK(p.isDirty == false);
  CHECK(p.pageId == 42);
  CHECK(p.numSlots() == 0);
  CHECK(p.freeSpace() == PAGE_SIZE - HEADER_SIZE);
}

TEST_CASE("insertRow returns slot index and updates numSlots") {
  Page p = makePage();
  auto row = makeRow(16);
  auto slot = p.insertRow(row.data(), row.size());
  REQUIRE(slot.has_value());
  CHECK(*slot == 0);
  CHECK(p.numSlots() == 1);
  CHECK(p.isDirty == true);

  auto row2 = makeRow(8, 0xCD);
  auto slot2 = p.insertRow(row2.data(), row2.size());
  REQUIRE(slot2.has_value());
  CHECK(*slot2 == 1);
  CHECK(p.numSlots() == 2);
}

TEST_CASE("insertRow reduces freeSpace by rowLen + SLOT_ENTRY_SIZE") {
  Page p = makePage();
  uint16_t before = p.freeSpace();
  auto row = makeRow(20);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  CHECK(p.freeSpace() == before - 20 - SLOT_ENTRY_SIZE);
}

TEST_CASE("insertRow returns InvalidRowLength for zero length row") {
  Page p = makePage();
  uint8_t dummy = 0;
  auto result = p.insertRow(&dummy, 0);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == PageError::InvalidRowLength);
  CHECK(p.numSlots() == 0);
}

TEST_CASE("insertRow returns PageFull when page is full") {
  Page p = makePage();
  auto row = makeRow(100);
  int inserted = 0;
  while (p.insertRow(row.data(), row.size()).has_value())
    inserted++;
  CHECK(inserted > 0);
  auto result = p.insertRow(row.data(), row.size());
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == PageError::PageFull);
}

TEST_CASE("readRow returns correct data") {
  Page p = makePage();
  auto row = makeRow(10, 0x12);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());

  uint16_t len = 0;
  auto result = p.readRow(0, &len);
  REQUIRE(result.has_value());
  CHECK(len == 10);
  CHECK(std::memcmp(*result, row.data(), 10) == 0);
}

TEST_CASE("readRow returns SlotOutOfBounds for out-of-bounds index") {
  Page p = makePage();
  uint16_t len = 0;
  CHECK(p.readRow(0, &len).error() == PageError::SlotOutOfBounds);
  auto row = makeRow(8);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  CHECK(p.readRow(1, &len).error() == PageError::SlotOutOfBounds);
}

TEST_CASE("readRow returns SlotDeleted for deleted slot") {
  Page p = makePage();
  auto row = makeRow(8);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  REQUIRE(p.deleteRow(0).has_value());

  uint16_t len = 0;
  CHECK(p.readRow(0, &len).error() == PageError::SlotDeleted);
}

TEST_CASE("deleteRow marks slot deleted and returns data pointer") {
  Page p = makePage();
  auto row = makeRow(12, 0x55);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());

  auto result = p.deleteRow(0);
  REQUIRE(result.has_value());
  CHECK(std::memcmp(*result, row.data(), 12) == 0);
  CHECK(p.slotDeleted(0) == true);
}

TEST_CASE("deleteRow returns SlotDeleted for already-deleted slot") {
  Page p = makePage();
  auto row = makeRow(8);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  REQUIRE(p.deleteRow(0).has_value());
  CHECK(p.deleteRow(0).error() == PageError::SlotDeleted);
}

TEST_CASE("deleteRow returns SlotOutOfBounds for out-of-bounds index") {
  Page p = makePage();
  CHECK(p.deleteRow(0).error() == PageError::SlotOutOfBounds);
}

TEST_CASE("updateRow overwrites data in place") {
  Page p = makePage();
  auto original = makeRow(16, 0xAA);
  auto updated  = makeRow(16, 0xBB);
  REQUIRE(p.insertRow(original.data(), original.size()).has_value());

  auto result = p.updateRow(0, updated.data(), updated.size());
  REQUIRE(result.has_value());

  uint16_t len = 0;
  auto read = p.readRow(0, &len);
  REQUIRE(read.has_value());
  CHECK(len == 16);
  CHECK(std::memcmp(*read, updated.data(), 16) == 0);
}

TEST_CASE("updateRow grows row into free space") {
  Page p = makePage();
  auto original = makeRow(16, 0xAA);
  auto bigger   = makeRow(20, 0xBB);
  REQUIRE(p.insertRow(original.data(), original.size()).has_value());

  auto result = p.updateRow(0, bigger.data(), bigger.size());
  REQUIRE(result.has_value());
  CHECK(std::memcmp(*result, bigger.data(), 20) == 0);

  uint16_t len = 0;
  auto read = p.readRow(0, &len);
  REQUIRE(read.has_value());
  CHECK(len == 20);
  CHECK(std::memcmp(*read, bigger.data(), 20) == 0);
}

TEST_CASE("updateRow grow updates free pointer so next insert does not corrupt it") {
  Page p = makePage();
  auto original = makeRow(16, 0xAA);
  auto bigger   = makeRow(20, 0xBB);
  REQUIRE(p.insertRow(original.data(), original.size()).has_value());
  REQUIRE(p.updateRow(0, bigger.data(), bigger.size()).has_value());

  auto newRow = makeRow(8, 0xCC);
  auto slot = p.insertRow(newRow.data(), newRow.size());
  REQUIRE(slot.has_value());
  CHECK(*slot == 1);

  uint16_t len = 0;
  auto read = p.readRow(0, &len);
  REQUIRE(read.has_value());
  CHECK(std::memcmp(*read, bigger.data(), 20) == 0);
}

TEST_CASE("updateRow returns PageFull when page has no space to grow row") {
  Page p = makePage();
  auto row = makeRow(100, 0xAA);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  auto filler = makeRow(100, 0xFF);
  while (p.insertRow(filler.data(), filler.size()).has_value()) {}

  auto bigger = makeRow(200, 0xBB);
  CHECK(p.updateRow(0, bigger.data(), bigger.size()).error() == PageError::PageFull);
}

TEST_CASE("updateRow returns SlotOutOfBounds for out-of-bounds index") {
  Page p = makePage();
  auto row = makeRow(8);
  CHECK(p.updateRow(0, row.data(), row.size()).error() == PageError::SlotOutOfBounds);
}

TEST_CASE("updateRow returns SlotDeleted for deleted slot") {
  Page p = makePage();
  auto row = makeRow(8, 0x11);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  REQUIRE(p.deleteRow(0).has_value());

  auto row2 = makeRow(8, 0x22);
  CHECK(p.updateRow(0, row2.data(), row2.size()).error() == PageError::SlotDeleted);
}

TEST_CASE("slotDeleted is false for live slot") {
  Page p = makePage();
  auto row = makeRow(8);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  CHECK(p.slotDeleted(0) == false);
}

TEST_CASE("multiple rows are independent") {
  Page p = makePage();
  auto r0 = makeRow(8, 0x11);
  auto r1 = makeRow(8, 0x22);
  auto r2 = makeRow(8, 0x33);
  REQUIRE(p.insertRow(r0.data(), r0.size()).has_value());
  REQUIRE(p.insertRow(r1.data(), r1.size()).has_value());
  REQUIRE(p.insertRow(r2.data(), r2.size()).has_value());

  uint16_t len = 0;
  CHECK(std::memcmp(*p.readRow(0, &len), r0.data(), 8) == 0);
  CHECK(std::memcmp(*p.readRow(1, &len), r1.data(), 8) == 0);
  CHECK(std::memcmp(*p.readRow(2, &len), r2.data(), 8) == 0);
}

TEST_CASE("delete middle slot does not affect others") {
  Page p = makePage();
  auto r0 = makeRow(8, 0x11);
  auto r1 = makeRow(8, 0x22);
  auto r2 = makeRow(8, 0x33);
  REQUIRE(p.insertRow(r0.data(), r0.size()).has_value());
  REQUIRE(p.insertRow(r1.data(), r1.size()).has_value());
  REQUIRE(p.insertRow(r2.data(), r2.size()).has_value());

  REQUIRE(p.deleteRow(1).has_value());

  uint16_t len = 0;
  CHECK(std::memcmp(*p.readRow(0, &len), r0.data(), 8) == 0);
  CHECK(p.readRow(1, &len).error() == PageError::SlotDeleted);
  CHECK(std::memcmp(*p.readRow(2, &len), r2.data(), 8) == 0);
}
