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
  int slot = p.insertRow(row.data(), row.size());
  CHECK(slot == 0);
  CHECK(p.numSlots() == 1);
  CHECK(p.isDirty == true);

  auto row2 = makeRow(8, 0xCD);
  int slot2 = p.insertRow(row2.data(), row2.size());
  CHECK(slot2 == 1);
  CHECK(p.numSlots() == 2);
}

TEST_CASE("insertRow reduces freeSpace by rowLen + SLOT_ENTRY_SIZE") {
  Page p = makePage();
  uint16_t before = p.freeSpace();
  auto row = makeRow(20);
  p.insertRow(row.data(), row.size());
  CHECK(p.freeSpace() == before - 20 - SLOT_ENTRY_SIZE);
}

TEST_CASE("insertRow returns -1 for zero length row") {
  Page p = makePage();
  uint8_t dummy = 0;
  CHECK(p.insertRow(&dummy, 0) == -1);
  CHECK(p.numSlots() == 0);
}

TEST_CASE("insertRow returns -1 when page is full") {
  Page p = makePage();
  // fill the page until it rejects an insert
  auto row = makeRow(100);
  int inserted = 0;
  while (p.insertRow(row.data(), row.size()) != -1)
    inserted++;
  CHECK(inserted > 0);
  CHECK(p.insertRow(row.data(), row.size()) == -1);
}

TEST_CASE("readRow returns correct data") {
  Page p = makePage();
  auto row = makeRow(10, 0x12);
  p.insertRow(row.data(), row.size());

  uint16_t len = 0;
  uint8_t *ptr = p.readRow(0, &len);
  REQUIRE(ptr != nullptr);
  CHECK(len == 10);
  CHECK(std::memcmp(ptr, row.data(), 10) == 0);
}

TEST_CASE("readRow returns nullptr for out-of-bounds index") {
  Page p = makePage();
  uint16_t len = 0;
  CHECK(p.readRow(0, &len) == nullptr);
  auto row = makeRow(8);
  p.insertRow(row.data(), row.size());
  CHECK(p.readRow(1, &len) == nullptr);
}

TEST_CASE("readRow returns nullptr for deleted slot") {
  Page p = makePage();
  auto row = makeRow(8);
  p.insertRow(row.data(), row.size());
  p.deleteRow(0);

  uint16_t len = 0;
  CHECK(p.readRow(0, &len) == nullptr);
}

TEST_CASE("deleteRow marks slot deleted and returns data pointer") {
  Page p = makePage();
  auto row = makeRow(12, 0x55);
  p.insertRow(row.data(), row.size());

  uint8_t *ptr = p.deleteRow(0);
  REQUIRE(ptr != nullptr);
  CHECK(std::memcmp(ptr, row.data(), 12) == 0);
  CHECK(p.slotDeleted(0) == true);
}

TEST_CASE("deleteRow returns nullptr for already-deleted slot") {
  Page p = makePage();
  auto row = makeRow(8);
  p.insertRow(row.data(), row.size());
  p.deleteRow(0);
  CHECK(p.deleteRow(0) == nullptr);
}

TEST_CASE("deleteRow returns nullptr for out-of-bounds index") {
  Page p = makePage();
  CHECK(p.deleteRow(0) == nullptr);
}

TEST_CASE("updateRow overwrites data in place") {
  Page p = makePage();
  auto original = makeRow(16, 0xAA);
  auto updated  = makeRow(16, 0xBB);
  p.insertRow(original.data(), original.size());

  uint8_t *ptr = p.updateRow(0, updated.data(), updated.size());
  REQUIRE(ptr != nullptr);

  uint16_t len = 0;
  uint8_t *read = p.readRow(0, &len);
  REQUIRE(read != nullptr);
  CHECK(len == 16);
  CHECK(std::memcmp(read, updated.data(), 16) == 0);
}

TEST_CASE("updateRow grows row into free space") {
  Page p = makePage();
  auto original = makeRow(16, 0xAA);
  auto bigger   = makeRow(20, 0xBB);
  p.insertRow(original.data(), original.size());

  uint8_t *ptr = p.updateRow(0, bigger.data(), bigger.size());
  REQUIRE(ptr != nullptr);
  CHECK(std::memcmp(ptr, bigger.data(), 20) == 0);

  uint16_t len = 0;
  uint8_t *read = p.readRow(0, &len);
  REQUIRE(read != nullptr);
  CHECK(len == 20);
  CHECK(std::memcmp(read, bigger.data(), 20) == 0);
}

TEST_CASE("updateRow grow updates free pointer so next insert does not corrupt it") {
  Page p = makePage();
  auto original = makeRow(16, 0xAA);
  auto bigger   = makeRow(20, 0xBB);
  p.insertRow(original.data(), original.size());
  p.updateRow(0, bigger.data(), bigger.size());

  auto newRow = makeRow(8, 0xCC);
  int slot = p.insertRow(newRow.data(), newRow.size());
  CHECK(slot == 1);

  uint16_t len = 0;
  uint8_t *read = p.readRow(0, &len);
  REQUIRE(read != nullptr);
  CHECK(std::memcmp(read, bigger.data(), 20) == 0);
}

TEST_CASE("updateRow returns nullptr when page has no space to grow row") {
  Page p = makePage();
  auto row = makeRow(100, 0xAA);
  p.insertRow(row.data(), row.size());
  auto filler = makeRow(100, 0xFF);
  while (p.insertRow(filler.data(), filler.size()) != -1) {}

  auto bigger = makeRow(200, 0xBB);
  CHECK(p.updateRow(0, bigger.data(), bigger.size()) == nullptr);
}

TEST_CASE("updateRow returns nullptr for out-of-bounds index") {
  Page p = makePage();
  auto row = makeRow(8);
  CHECK(p.updateRow(0, row.data(), row.size()) == nullptr);
}

TEST_CASE("updateRow returns nullptr for deleted slot") {
  Page p = makePage();
  auto row = makeRow(8, 0x11);
  p.insertRow(row.data(), row.size());
  p.deleteRow(0);

  auto row2 = makeRow(8, 0x22);
  CHECK(p.updateRow(0, row2.data(), row2.size()) == nullptr);
}

TEST_CASE("slotDeleted is false for live slot") {
  Page p = makePage();
  auto row = makeRow(8);
  p.insertRow(row.data(), row.size());
  CHECK(p.slotDeleted(0) == false);
}

TEST_CASE("multiple rows are independent") {
  Page p = makePage();
  auto r0 = makeRow(8, 0x11);
  auto r1 = makeRow(8, 0x22);
  auto r2 = makeRow(8, 0x33);
  p.insertRow(r0.data(), r0.size());
  p.insertRow(r1.data(), r1.size());
  p.insertRow(r2.data(), r2.size());

  uint16_t len = 0;
  CHECK(std::memcmp(p.readRow(0, &len), r0.data(), 8) == 0);
  CHECK(std::memcmp(p.readRow(1, &len), r1.data(), 8) == 0);
  CHECK(std::memcmp(p.readRow(2, &len), r2.data(), 8) == 0);
}

TEST_CASE("delete middle slot does not affect others") {
  Page p = makePage();
  auto r0 = makeRow(8, 0x11);
  auto r1 = makeRow(8, 0x22);
  auto r2 = makeRow(8, 0x33);
  p.insertRow(r0.data(), r0.size());
  p.insertRow(r1.data(), r1.size());
  p.insertRow(r2.data(), r2.size());

  p.deleteRow(1);

  uint16_t len = 0;
  CHECK(std::memcmp(p.readRow(0, &len), r0.data(), 8) == 0);
  CHECK(p.readRow(1, &len) == nullptr);
  CHECK(std::memcmp(p.readRow(2, &len), r2.data(), 8) == 0);
}
