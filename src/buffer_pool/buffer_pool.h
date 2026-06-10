#pragma once

#include "buffer_pool/frame.h"
#include <expected>
#include <unordered_map>
inline constexpr int BUFFER_POOL_CAPACITY = 64;

enum class BufferPoolError {
  BufferPoolFull,
  ErrorReadingPage,
  ErrorWritingPage,
  ErrorAllocatingNewPage,
  ErrorAllFramesPinned
};

struct BufferPool {
  Frame frames[BUFFER_POOL_CAPACITY];
  std::unordered_map<std::string, TableFile *> openFiles;
  uint64_t clock = 0; // increments by 1 after every page access

  std::expected<Page *, BufferPoolError>
  fetchPage(const std::string &tableName,
            uint32_t pageId); //  load from disk if needed
  void unpinPage(const std::string &tableName, uint32_t pageId, bool dirty);

  std::expected<Page *, BufferPoolError> newPage(const std::string &tableName);

  std::expected<void, BufferPoolError>
  flushPage(const std::string &tableName,
            uint32_t pageId); // force save one specific page to disk
  std::expected<void, BufferPoolError>
  flushAll();         // force save all dirty pages to disk
  int findLruFrame(); // helper that picks the best frame to evict
                      // (returns frame index, or -1 if all pinned)
};
