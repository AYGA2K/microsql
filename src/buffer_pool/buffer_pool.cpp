#include "buffer_pool.h"
#include "buffer_pool/frame.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <print>

static const char *tableFileErrorStr(TableFileError e) {
  switch (e) {
  case TableFileError::FileNotOpen:
    return "FileNotOpen";
  case TableFileError::FailedToOpenFile:
    return "FailedToOpenFile";
  case TableFileError::FailedToSeekPage:
    return "FailedToSeekPage";
  case TableFileError::FailedToReadPage:
    return "FailedToReadPage";
  case TableFileError::FailedToWritePage:
    return "FailedToWritePage";
  case TableFileError::FailedToGetFileSize:
    return "FailedToGetFileSize";
  }
  return "Unknown";
}

std::expected<Page *, BufferPoolError>
BufferPool::fetchPage(const std::string &tableName, uint32_t pageId) {

  // Check if the page exists in cache
  for (size_t i = 0; i < BUFFER_POOL_CAPACITY; i++) {
    Frame &frame = this->frames[i];
    if (frame.occupied && frame.tableName == tableName &&
        frame.pageId == pageId) {
      frame.pinCount++;
      frame.lastUsed = this->clock;
      this->clock++;
      return frame.page;
    }
  }

  int index = this->findLruFrame();
  if (index == -1) {
    return std::unexpected(BufferPoolError::BufferPoolFull);
  }
  Frame &frame = this->frames[index];
  // If the frame is dirty write its page first to disk before updating the
  // frame with the new fetched page
  if (frame.occupied && frame.isDirty) {
    TableFile *tableFile = openFiles.at(frame.tableName);
    auto ok = tableFile->writePage(*frame.page);
    if (!ok) {
      std::println(
          stderr, "[BufferPool] failed to evict page {} of table '{}': {}",
          frame.pageId, frame.tableName, tableFileErrorStr(ok.error()));
      return std::unexpected(BufferPoolError::ErrorWritingPage);
    }
  }

  TableFile *tableFile = openFiles.at(tableName);
  auto page = tableFile->readPage(pageId);
  if (!page) {
    std::println(stderr,
                 "[BufferPool] failed to read page {} of table '{}': {}",
                 pageId, tableName, tableFileErrorStr(page.error()));
    return std::unexpected(BufferPoolError::ErrorReadingPage);
  }
  frame.page = page.value();
  frame.pageId = pageId;
  frame.tableName = tableName;
  frame.isDirty = false;
  frame.occupied = true;
  frame.pinCount = 1;
  frame.lastUsed = this->clock;
  this->clock++;
  return page.value();
}

int BufferPool::findLruFrame() {
  uint64_t minLastUsed = UINT64_MAX;
  int index = -1;
  for (size_t i = 0; i < BUFFER_POOL_CAPACITY; i++) {
    const Frame &frame = this->frames[i];

    // Return the index of first unoccupied frame
    if (!frame.occupied) {
      return i;
    }

    // Get the unused frame with the smallest last used timestamp
    if (frame.pinCount == 0 && frame.lastUsed < minLastUsed) {
      minLastUsed = frame.lastUsed;
      index = i;
    }
  }
  return index;
}

void BufferPool::unpinPage(const std::string &tableName, uint32_t pageId,
                           bool dirty) {
  for (size_t i = 0; i < BUFFER_POOL_CAPACITY; i++) {
    Frame &frame = this->frames[i];
    if (frame.occupied && frame.tableName == tableName &&
        frame.pageId == pageId) {
      frame.pinCount--;
      frame.isDirty = frame.isDirty || dirty;
    }
  }
}

std::expected<Page *, BufferPoolError>
BufferPool::newPage(const std::string &tableName) {
  int victim = this->findLruFrame();
  if (victim == -1) {
    return std::unexpected(BufferPoolError::ErrorAllFramesPinned);
  }
  TableFile *tableFile = this->openFiles.at(tableName);
  auto pageId = tableFile->allocatePage();
  if (!pageId) {
    return std::unexpected(BufferPoolError::ErrorAllocatingNewPage);
  }
  Frame &frame = frames[victim];
  if (frame.occupied && frame.isDirty) {
    TableFile *evicteeFile = openFiles[frame.tableName];
    auto ok = evicteeFile->writePage(*frame.page);
    if (!ok) {
      return std::unexpected(BufferPoolError::ErrorWritingPage);
    }
  }
  Page *page = new Page();
  page->init(pageId.value());
  frame.page = page;
  frame.tableName = tableName;
  frame.pageId = page->pageId;
  frame.occupied = true;
  frame.pinCount = 1;
  frame.isDirty = true;
  frame.lastUsed = this->clock;
  this->clock++;
  return page;
}

std::expected<void, BufferPoolError>
BufferPool::flushPage(const std::string &tableName, uint32_t pageId) {
  TableFile *tablefile = this->openFiles.at(tableName);
  for (size_t i = 0; i < BUFFER_POOL_CAPACITY; i++) {
    Frame &frame = this->frames[i];
    if (frame.tableName == tableName && frame.pageId == pageId &&
        frame.isDirty) {
      auto ok = tablefile->writePage(*frame.page);
      if (!ok) {
        return std::unexpected(BufferPoolError::ErrorWritingPage);
      }
    }
  }
  return {};
}

std::expected<void, BufferPoolError> BufferPool::flushAll() {
  for (size_t i = 0; i < BUFFER_POOL_CAPACITY; i++) {
    Frame &frame = this->frames[i];
    if (frame.isDirty) {
      TableFile *tablefile = this->openFiles.at(frame.tableName);
      auto ok = tablefile->writePage(*frame.page);
      if (!ok) {
        return std::unexpected(BufferPoolError::ErrorWritingPage);
      }
    }
  }
  return {};
}
