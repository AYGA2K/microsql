#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "storage/page.h"
#include <cstring>
#include <doctest/doctest.h>
#include <unistd.h>

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
  CHECK(p.pageId == 42);
  CHECK(p.numSlots() == 0);
  CHECK(p.freeSpace() == PAGE_SIZE - HEADER_SIZE);
}

TEST_CASE("insertRow updates numSlots and stores row data") {
  Page p = makePage();
  auto row = makeRow(16);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  CHECK(p.numSlots() == 1);
  uint16_t len0 = 0;
  auto read0 = p.readRow(0, &len0);
  REQUIRE(read0.has_value());
  CHECK(std::memcmp(*read0, row.data(), 16) == 0);

  auto row2 = makeRow(8, 0xCD);
  REQUIRE(p.insertRow(row2.data(), row2.size()).has_value());
  CHECK(p.numSlots() == 2);
  uint16_t len1 = 0;
  auto read1 = p.readRow(1, &len1);
  REQUIRE(read1.has_value());
  CHECK(std::memcmp(*read1, row2.data(), 8) == 0);
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

TEST_CASE("deleteRow marks slot as deleted") {
  Page p = makePage();
  auto row = makeRow(12, 0x55);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());

  REQUIRE(p.deleteRow(0).has_value());
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
  auto updated = makeRow(16, 0xBB);
  REQUIRE(p.insertRow(original.data(), original.size()).has_value());

  auto result = p.updateRow(0, updated.data(), updated.size());
  REQUIRE(result.has_value());

  uint16_t len = 0;
  auto read = p.readRow(0, &len);
  REQUIRE(read.has_value());
  CHECK(len == 16);
  CHECK(std::memcmp(*read, updated.data(), 16) == 0);
}


TEST_CASE("updateRow returns SlotOutOfBounds for out-of-bounds index") {
  Page p = makePage();
  auto row = makeRow(8);
  CHECK(p.updateRow(0, row.data(), row.size()).error() ==
        PageError::SlotOutOfBounds);
}

TEST_CASE("updateRow returns SlotDeleted for deleted slot") {
  Page p = makePage();
  auto row = makeRow(8, 0x11);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  REQUIRE(p.deleteRow(0).has_value());

  auto row2 = makeRow(8, 0x22);
  CHECK(p.updateRow(0, row2.data(), row2.size()).error() ==
        PageError::SlotDeleted);
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

TEST_CASE("updateRow returns InvalidRowLength for zero-length row") {
  Page p = makePage();
  auto row = makeRow(8, 0x11);
  REQUIRE(p.insertRow(row.data(), row.size()).has_value());
  uint8_t dummy = 0;
  auto result = p.updateRow(0, &dummy, 0);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == PageError::InvalidRowLength);
}

TEST_CASE("updateRow shrinks row in place") {
  Page p = makePage();
  auto original = makeRow(16, 0xAA);
  auto smaller = makeRow(8, 0xBB);
  REQUIRE(p.insertRow(original.data(), original.size()).has_value());

  REQUIRE(p.updateRow(0, smaller.data(), smaller.size()).has_value());

  uint16_t len = 0;
  auto read = p.readRow(0, &len);
  REQUIRE(read.has_value());
  CHECK(len == 8);
  CHECK(std::memcmp(*read, smaller.data(), 8) == 0);
}

struct TempFile {
  std::string path;
  TempFile() {
    char buf[] = "/tmp/microsql_test_XXXXXX";
    int fd = mkstemp(buf);
    if (fd != -1)
      ::close(fd);
    path = buf;
  }
  ~TempFile() { std::remove(path.c_str()); }
  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
};

TEST_CASE("TableFile::open fails on non-existent path") {
  TableFile tf;
  auto result = tf.open("/tmp/microsql_no_such_file_xyz.db");
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == TableFileError::FailedToOpenFile);
}

TEST_CASE("TableFile::open succeeds on existing file") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  tf.close();
}

TEST_CASE("TableFile::close on unopened TableFile does not crash") {
  TableFile tf;
  tf.close();
}

TEST_CASE("TableFile::close twice does not crash") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  tf.close();
  tf.close();
}

TEST_CASE("TableFile::numPages returns FileNotOpen when file is not open") {
  TableFile tf;
  auto result = tf.numPages();
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == TableFileError::FileNotOpen);
}

TEST_CASE("TableFile::numPages returns 0 for empty file") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  auto result = tf.numPages();
  REQUIRE(result.has_value());
  CHECK(*result == 0);
  tf.close();
}

TEST_CASE("TableFile::numPages reflects each allocated page") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  for (size_t i = 1; i <= 4; i++) {
    REQUIRE(tf.allocatePage().has_value());
    auto n = tf.numPages();
    REQUIRE(n.has_value());
    CHECK(*n == i);
  }
  tf.close();
}

TEST_CASE("TableFile::allocatePage returns FileNotOpen when file is not open") {
  TableFile tf;
  auto result = tf.allocatePage();
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == TableFileError::FileNotOpen);
}

TEST_CASE("TableFile::allocatePage first call returns page id 0") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  auto result = tf.allocatePage();
  REQUIRE(result.has_value());
  CHECK(*result == 0);
  tf.close();
}

TEST_CASE("TableFile::allocatePage returns sequential ids") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  for (uint32_t i = 0; i < 5; i++) {
    auto result = tf.allocatePage();
    REQUIRE(result.has_value());
    CHECK(*result == i);
  }
  tf.close();
}

TEST_CASE("TableFile::writePage returns FileNotOpen when file is not open") {
  Page p = makePage(0);
  TableFile tf;
  auto result = tf.writePage(p);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == TableFileError::FileNotOpen);
}

TEST_CASE("TableFile::readPage returns FileNotOpen when file is not open") {
  TableFile tf;
  auto result = tf.readPage(0);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == TableFileError::FileNotOpen);
}

TEST_CASE("TableFile::readPage fails on out-of-bounds page id") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  auto result = tf.readPage(0);
  REQUIRE_FALSE(result.has_value());
  tf.close();
}

TEST_CASE("TableFile write then read preserves page contents") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  REQUIRE(tf.allocatePage().has_value());

  Page written = makePage(0);
  auto row = makeRow(32, 0xAB);
  REQUIRE(written.insertRow(row.data(), row.size()).has_value());
  REQUIRE(tf.writePage(written).has_value());

  auto readResult = tf.readPage(0);
  REQUIRE(readResult.has_value());
  Page *read = *readResult;
  CHECK(read->pageId == 0);
  CHECK(read->numSlots() == 1);

  uint16_t len = 0;
  auto data = read->readRow(0, &len);
  REQUIRE(data.has_value());
  CHECK(len == 32);
  CHECK(std::memcmp(*data, row.data(), 32) == 0);
  delete read;
  tf.close();
}

TEST_CASE("TableFile multiple pages are stored independently") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  REQUIRE(tf.allocatePage().has_value());
  REQUIRE(tf.allocatePage().has_value());

  Page p0 = makePage(0);
  auto row0 = makeRow(8, 0x11);
  REQUIRE(p0.insertRow(row0.data(), row0.size()).has_value());
  REQUIRE(tf.writePage(p0).has_value());

  Page p1 = makePage(1);
  auto row1 = makeRow(8, 0x22);
  REQUIRE(p1.insertRow(row1.data(), row1.size()).has_value());
  REQUIRE(tf.writePage(p1).has_value());

  uint16_t len = 0;
  auto r0Result = tf.readPage(0);
  REQUIRE(r0Result.has_value());
  Page *r0 = *r0Result;
  auto d0 = r0->readRow(0, &len);
  REQUIRE(d0.has_value());
  CHECK(std::memcmp(*d0, row0.data(), 8) == 0);
  delete r0;

  auto r1Result = tf.readPage(1);
  REQUIRE(r1Result.has_value());
  Page *r1 = *r1Result;
  auto d1 = r1->readRow(0, &len);
  REQUIRE(d1.has_value());
  CHECK(std::memcmp(*d1, row1.data(), 8) == 0);
  delete r1;

  tf.close();
}

TEST_CASE("TableFile overwriting a page replaces its content") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  REQUIRE(tf.allocatePage().has_value());

  Page first = makePage(0);
  auto row1 = makeRow(8, 0xAA);
  REQUIRE(first.insertRow(row1.data(), row1.size()).has_value());
  REQUIRE(tf.writePage(first).has_value());

  Page second = makePage(0);
  auto row2 = makeRow(8, 0xBB);
  REQUIRE(second.insertRow(row2.data(), row2.size()).has_value());
  REQUIRE(tf.writePage(second).has_value());

  auto readResult = tf.readPage(0);
  REQUIRE(readResult.has_value());
  Page *read = *readResult;
  uint16_t len = 0;
  auto data = read->readRow(0, &len);
  REQUIRE(data.has_value());
  CHECK(std::memcmp(*data, row2.data(), 8) == 0);
  delete read;
  tf.close();
}

TEST_CASE("TableFile data persists across close and reopen") {
  TempFile tmp;
  {
    TableFile tf;
    REQUIRE(tf.open(tmp.path).has_value());
    REQUIRE(tf.allocatePage().has_value());

    Page p = makePage(0);
    auto row = makeRow(16, 0xCC);
    REQUIRE(p.insertRow(row.data(), row.size()).has_value());
    REQUIRE(tf.writePage(p).has_value());
    tf.close();
  }
  {
    TableFile tf;
    REQUIRE(tf.open(tmp.path).has_value());

    auto pages = tf.numPages();
    REQUIRE(pages.has_value());
    CHECK(*pages == 1);

    auto pResult = tf.readPage(0);
    REQUIRE(pResult.has_value());
    Page *p = *pResult;
    uint16_t len = 0;
    auto data = p->readRow(0, &len);
    REQUIRE(data.has_value());
    CHECK(len == 16);
    auto expected = makeRow(16, 0xCC);
    CHECK(std::memcmp(*data, expected.data(), 16) == 0);
    delete p;
    tf.close();
  }
}

TEST_CASE("TableFile readPage loads page data from disk") {
  TempFile tmp;
  TableFile tf;
  REQUIRE(tf.open(tmp.path).has_value());
  REQUIRE(tf.allocatePage().has_value());

  Page written = makePage(0);
  auto row = makeRow(8, 0x77);
  REQUIRE(written.insertRow(row.data(), row.size()).has_value());
  REQUIRE(tf.writePage(written).has_value());

  auto readResult = tf.readPage(0);
  REQUIRE(readResult.has_value());
  Page *read = *readResult;
  CHECK(read->pageId == 0);
  delete read;
  tf.close();
}
