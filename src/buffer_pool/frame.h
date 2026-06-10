#pragma once
#include "storage/page.h"
#include <cstdint>
#include <string>

struct Frame {
  Page *page;
  std::string tableName;
  uint32_t pageId;
  int pinCount; // how many callers are currently using this frame
  bool isDirty; // whether this page has been modified since it was last written
                // to disk
  bool occupied;
  uint64_t lastUsed;
};
