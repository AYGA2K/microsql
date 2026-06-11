#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "buffer_pool/buffer_pool.h"
#include <doctest/doctest.h>
#include <unistd.h>

struct TempFile {
  std::string path;
  TempFile() {
    char buf[] = "/tmp/microsql_bp_test_XXXXXX";
    int fd = mkstemp(buf);
    if (fd != -1)
      ::close(fd);
    path = buf;
  }
  ~TempFile() { std::remove(path.c_str()); }
  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
};

// Open a TableFile, pre-allocate `numPages` pages, and register it in the pool.
static TableFile *setupTable(BufferPool &bp, const std::string &name,
                             const std::string &path, int numPages = 1) {
  TableFile *tf = new TableFile{};
  REQUIRE(tf->open(path).has_value());
  for (int i = 0; i < numPages; i++)
    REQUIRE(tf->allocatePage().has_value());
  bp.openFiles[name] = tf;
  return tf;
}

TEST_CASE("fetchPage returns same pointer on cache hit") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path);

  auto p1 = bp.fetchPage("t", 0);
  REQUIRE(p1.has_value());
  auto p2 = bp.fetchPage("t", 0);
  REQUIRE(p2.has_value());
  CHECK(p1.value() == p2.value());
}

TEST_CASE("fetchPage increments pin count on cache hit") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path);

  REQUIRE(bp.fetchPage("t", 0).has_value());
  REQUIRE(bp.fetchPage("t", 0).has_value());

  int pins = 0;
  for (auto &f : bp.frames)
    if (f.occupied && f.tableName == "t" && f.pageId == 0)
      pins = f.pinCount;
  CHECK(pins == 2);
}

TEST_CASE("fetchPage loads page data from disk") {
  TempFile tmp;
  BufferPool bp{};
  TableFile *tf = setupTable(bp, "t", tmp.path);

  // Write a known row directly via TableFile
  Page written;
  written.init(0);
  uint8_t row[] = {0xDE, 0xAD, 0xBE, 0xEF};
  REQUIRE(written.insertRow(row, 4).has_value());
  REQUIRE(tf->writePage(written).has_value());

  auto result = bp.fetchPage("t", 0);
  REQUIRE(result.has_value());
  uint16_t len = 0;
  auto data = result.value()->readRow(0, &len);
  REQUIRE(data.has_value());
  CHECK(len == 4);
  CHECK(std::memcmp(*data, row, 4) == 0);
}

TEST_CASE("unpinPage decrements pin count") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path);

  REQUIRE(bp.fetchPage("t", 0).has_value());
  REQUIRE(bp.fetchPage("t", 0).has_value());
  bp.unpinPage("t", 0, false);

  int pins = 0;
  for (auto &f : bp.frames)
    if (f.occupied && f.tableName == "t" && f.pageId == 0)
      pins = f.pinCount;
  CHECK(pins == 1);
}

TEST_CASE("unpinPage with dirty=true marks frame dirty") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path);

  REQUIRE(bp.fetchPage("t", 0).has_value());
  bp.unpinPage("t", 0, true);

  bool dirty = false;
  for (auto &f : bp.frames)
    if (f.occupied && f.tableName == "t" && f.pageId == 0)
      dirty = f.isDirty;
  CHECK(dirty == true);
}

TEST_CASE("unpinPage with dirty=false does not clear existing dirty flag") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path);

  REQUIRE(bp.fetchPage("t", 0).has_value());
  bp.unpinPage("t", 0, true);  // mark dirty
  REQUIRE(bp.fetchPage("t", 0).has_value());
  bp.unpinPage("t", 0, false); // should NOT clear it

  bool dirty = false;
  for (auto &f : bp.frames)
    if (f.occupied && f.tableName == "t" && f.pageId == 0)
      dirty = f.isDirty;
  CHECK(dirty == true);
}

TEST_CASE("newPage allocates a page and returns it pinned") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path, 0);

  auto result = bp.newPage("t");
  REQUIRE(result.has_value());
  Page *p = result.value();
  CHECK(p != nullptr);

  int pins = 0;
  for (auto &f : bp.frames)
    if (f.occupied && f.tableName == "t")
      pins = f.pinCount;
  CHECK(pins == 1);
}

TEST_CASE("newPage marks frame dirty") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path, 0);

  REQUIRE(bp.newPage("t").has_value());

  bool dirty = false;
  for (auto &f : bp.frames)
    if (f.occupied && f.tableName == "t")
      dirty = f.isDirty;
  CHECK(dirty == true);
}

TEST_CASE("flushPage writes dirty page to disk") {
  TempFile tmp;
  BufferPool bp{};
  TableFile *tf = setupTable(bp, "t", tmp.path, 0);

  auto result = bp.newPage("t");
  REQUIRE(result.has_value());
  uint32_t pid = result.value()->pageId;
  uint8_t row[] = {0x01, 0x02, 0x03};
  REQUIRE(result.value()->insertRow(row, 3).has_value());
  bp.unpinPage("t", pid, true);

  REQUIRE(bp.flushPage("t", pid).has_value());

  // Read back directly from disk to confirm the write happened
  auto readResult = tf->readPage(pid);
  REQUIRE(readResult.has_value());
  uint16_t len = 0;
  auto data = (*readResult)->readRow(0, &len);
  REQUIRE(data.has_value());
  CHECK(len == 3);
  CHECK(std::memcmp(*data, row, 3) == 0);
  delete *readResult;
}

TEST_CASE("flushAll writes all dirty pages to disk") {
  TempFile tmp1, tmp2;
  BufferPool bp{};
  TableFile *tf1 = setupTable(bp, "a", tmp1.path, 0);
  TableFile *tf2 = setupTable(bp, "b", tmp2.path, 0);

  auto pa = bp.newPage("a");
  REQUIRE(pa.has_value());
  auto pb = bp.newPage("b");
  REQUIRE(pb.has_value());

  uint8_t rowA[] = {0xAA};
  uint8_t rowB[] = {0xBB};
  REQUIRE(pa.value()->insertRow(rowA, 1).has_value());
  REQUIRE(pb.value()->insertRow(rowB, 1).has_value());
  bp.unpinPage("a", pa.value()->pageId, true);
  bp.unpinPage("b", pb.value()->pageId, true);

  REQUIRE(bp.flushAll().has_value());

  auto ra = tf1->readPage(pa.value()->pageId);
  REQUIRE(ra.has_value());
  uint16_t lenA = 0;
  CHECK((*ra)->readRow(0, &lenA).has_value());
  delete *ra;

  auto rb = tf2->readPage(pb.value()->pageId);
  REQUIRE(rb.has_value());
  uint16_t lenB = 0;
  CHECK((*rb)->readRow(0, &lenB).has_value());
  delete *rb;
}

TEST_CASE("dirty frame is flushed to disk on eviction") {
  TempFile tmp;
  BufferPool bp{};
  TableFile *tf = setupTable(bp, "t", tmp.path, BUFFER_POOL_CAPACITY + 1);

  // Fill the pool and unpin all frames (LRU order: page 0 is oldest)
  for (int i = 0; i < BUFFER_POOL_CAPACITY; i++) {
    REQUIRE(bp.fetchPage("t", i).has_value());
    bp.unpinPage("t", i, true);
  }

  // Fetching one more page forces eviction of the LRU (page 0)
  REQUIRE(bp.fetchPage("t", BUFFER_POOL_CAPACITY).has_value());

  // Page 0 should have been flushed — read it from disk to confirm it exists
  auto result = tf->readPage(0);
  CHECK(result.has_value());
  if (result.has_value())
    delete *result;
}

TEST_CASE("fetchPage returns BufferPoolFull when all frames are pinned") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path, BUFFER_POOL_CAPACITY + 1);

  // Pin every frame without unpinning
  for (int i = 0; i < BUFFER_POOL_CAPACITY; i++)
    REQUIRE(bp.fetchPage("t", i).has_value());

  auto result = bp.fetchPage("t", BUFFER_POOL_CAPACITY);
  REQUIRE_FALSE(result.has_value());
  CHECK(result.error() == BufferPoolError::BufferPoolFull);
}

TEST_CASE("LRU eviction picks the least recently used unpinned frame") {
  TempFile tmp;
  BufferPool bp{};
  setupTable(bp, "t", tmp.path, BUFFER_POOL_CAPACITY + 1);

  for (int i = 0; i < BUFFER_POOL_CAPACITY; i++) {
    REQUIRE(bp.fetchPage("t", i).has_value());
    bp.unpinPage("t", i, false);
  }

  // Re-access page 1..N-1 to make page 0 the LRU
  for (int i = 1; i < BUFFER_POOL_CAPACITY; i++) {
    REQUIRE(bp.fetchPage("t", i).has_value());
    bp.unpinPage("t", i, false);
  }

  // Bring in one new page — page 0 should be the evicted frame
  REQUIRE(bp.fetchPage("t", BUFFER_POOL_CAPACITY).has_value());

  bool page0InPool = false;
  for (auto &f : bp.frames)
    if (f.occupied && f.tableName == "t" && f.pageId == 0)
      page0InPool = true;
  CHECK_FALSE(page0InPool);
}
