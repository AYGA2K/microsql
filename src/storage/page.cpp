#include "page.h"
#include "expected"
#include <cstddef>
#include <cstdint>
#include <cstring>

uint16_t Page::read16(size_t offset) const {
  uint16_t value{};
  std::memcpy(&value, data + offset, 2);
  return value;
}

void Page::write16(size_t offset, uint16_t value) {
  std::memcpy(data + offset, &value, 2);
}

uint16_t Page::numSlots() const {
  return this->read16(HEADER_OFFSET_NUM_SLOTS);
}

uint16_t Page::freeSpace() const {
  uint16_t freeSpacePtr = this->read16(HEADER_OFFSET_FREE_PTR);
  uint16_t slots = this->numSlots();
  return freeSpacePtr - (HEADER_SIZE + slots * SLOT_ENTRY_SIZE);
}

bool Page::slotDeleted(uint16_t slotIndex) const {
  int slot = HEADER_SIZE + slotIndex * SLOT_ENTRY_SIZE;
  return this->read16(slot + 2) == 0;
}

void Page::init(uint32_t id) {
  this->isDirty = false;
  this->pageId = id;
  std::memset(this->data, 0, PAGE_SIZE);
  this->write16(HEADER_OFFSET_NUM_SLOTS, 0);
  this->write16(HEADER_OFFSET_FREE_PTR, PAGE_SIZE);
}

std::expected<int, PageError> Page::insertRow(const uint8_t *rowBytes,
                                              uint16_t rowLen) {
  if (rowLen == 0) {
    return std::unexpected(PageError::InvalidRowLength);
  }

  uint16_t slots = this->numSlots();
  uint16_t freeptr = this->read16(HEADER_OFFSET_FREE_PTR);
  int slotDirEnd = HEADER_SIZE + (slots * SLOT_ENTRY_SIZE);

  if (this->freeSpace() < rowLen + SLOT_ENTRY_SIZE) {
    return std::unexpected(PageError::PageFull);
  }

  uint16_t offset = freeptr - rowLen;

  std::memcpy(data + offset, rowBytes, rowLen);

  this->write16(slotDirEnd, offset);
  this->write16(slotDirEnd + 2, rowLen);

  this->write16(HEADER_OFFSET_FREE_PTR, offset);
  this->write16(HEADER_OFFSET_NUM_SLOTS, slots + 1);

  this->isDirty = true;
  return slots;
}

std::expected<uint8_t *, PageError> Page::readRow(uint16_t slotIndex,
                                                  uint16_t *rowLen) {
  if (slotIndex >= this->numSlots()) {
    return std::unexpected(PageError::SlotOutOfBounds);
  }
  if (this->slotDeleted(slotIndex)) {
    return std::unexpected(PageError::SlotDeleted);
  }

  int slot = HEADER_SIZE + slotIndex * SLOT_ENTRY_SIZE;
  uint16_t rowOffset = this->read16(slot);
  *rowLen = this->read16(slot + 2);

  return this->data + rowOffset;
}

std::expected<uint8_t *, PageError> Page::deleteRow(uint16_t slotIndex) {
  if (slotIndex >= this->numSlots()) {
    return std::unexpected(PageError::SlotOutOfBounds);
  }
  if (this->slotDeleted(slotIndex)) {
    return std::unexpected(PageError::SlotDeleted);
  }

  int slot = HEADER_SIZE + slotIndex * SLOT_ENTRY_SIZE;
  uint16_t rowOffset = this->read16(slot);

  this->write16(slot + 2, 0);

  return this->data + rowOffset;
}

std::expected<uint8_t *, PageError>
Page::updateRow(uint16_t slotIndex, const uint8_t *rowBytes, uint16_t rowLen) {
  if (slotIndex >= this->numSlots()) {
    return std::unexpected(PageError::SlotOutOfBounds);
  }
  if (this->slotDeleted(slotIndex)) {
    return std::unexpected(PageError::SlotDeleted);
  }
  if (rowLen == 0) {
    return std::unexpected(PageError::InvalidRowLength);
  }

  int slotOffset = HEADER_SIZE + slotIndex * SLOT_ENTRY_SIZE;
  uint16_t oldRowOffset = this->read16(slotOffset);
  uint16_t oldRowLen = this->read16(slotOffset + 2);

  // If the new row length > than the old row => insert the new updated row at
  // the top and update the slot data in the slot entry
  if (rowLen > oldRowLen) {
    if (this->freeSpace() < rowLen) {
      return std::unexpected(PageError::PageFull);
    }
    uint16_t freeptr = this->read16(HEADER_OFFSET_FREE_PTR);
    uint16_t newRowOffset = freeptr - rowLen;
    // Insert the new row
    std::memcpy(data + newRowOffset, rowBytes, rowLen);
    // Update the slot data
    this->write16(slotOffset, newRowOffset);
    this->write16(slotOffset + 2, rowLen);
    this->write16(HEADER_OFFSET_FREE_PTR, newRowOffset);
    oldRowOffset = newRowOffset;
  } else {
    std::memcpy(data + oldRowOffset, rowBytes, rowLen);
    this->write16(slotOffset + 2, rowLen);
  }

  this->isDirty = true;
  return this->data + oldRowOffset;
}

std::expected<void, TableFileError> TableFile::open(const std::string &path) {
  this->filePath = path;
  this->file.open(path, std::ios::binary | std::ios::in | std::ios::out);
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToOpenFile);
  }
  return {};
}

void TableFile::close() {
  if (this->file.is_open()) {
    this->file.flush();
    this->file.close();
  }
}

std::expected<Page *, TableFileError> TableFile::readPage(uint32_t pageId) {
  if (!this->file.is_open()) {
    return std::unexpected(TableFileError::FileNotOpen);
  }
  this->file.seekg(static_cast<std::streamoff>(pageId) * PAGE_SIZE,
                   std::ios::beg);
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToSeekPage);
  }
  Page *page = new Page{};
  this->file.read(reinterpret_cast<char *>(page->data), PAGE_SIZE);
  if (!this->file) {
    delete page;
    return std::unexpected(TableFileError::FailedToReadPage);
  }
  page->pageId = pageId;
  page->isDirty = false;
  return page;
}

std::expected<void, TableFileError> TableFile::writePage(const Page &page) {
  if (!this->file.is_open()) {
    return std::unexpected(TableFileError::FileNotOpen);
  }
  this->file.seekg(static_cast<std::streamoff>(page.pageId) * PAGE_SIZE,
                   std::ios::beg);
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToSeekPage);
  }
  this->file.write(reinterpret_cast<const char *>(page.data), PAGE_SIZE);
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToWritePage);
  }
  file.flush();
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToWritePage);
  }
  return {};
}

std::expected<size_t, TableFileError> TableFile::numPages() {
  if (!this->file.is_open()) {
    return std::unexpected(TableFileError::FileNotOpen);
  }

  this->file.clear();
  this->file.seekg(0, std::ios::end);
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToSeekPage);
  }

  auto pos = this->file.tellg();
  if (pos == std::streampos(-1)) {
    return std::unexpected(TableFileError::FailedToGetFileSize);
  }
  std::size_t size = static_cast<std::size_t>(pos);
  return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

std::expected<uint32_t, TableFileError> TableFile::allocatePage() {
  if (!this->file.is_open()) {
    return std::unexpected(TableFileError::FileNotOpen);
  }

  this->file.clear();
  this->file.seekp(0, std::ios::end);
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToSeekPage);
  }

  std::array<char, PAGE_SIZE> page{};
  this->file.write(page.data(), PAGE_SIZE);
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToWritePage);
  }

  this->file.flush();
  if (!this->file) {
    return std::unexpected(TableFileError::FailedToWritePage);
  }

  auto pages = this->numPages();
  if (!pages) {
    return std::unexpected(pages.error());
  }

  return static_cast<uint32_t>(pages.value() - 1);
}
