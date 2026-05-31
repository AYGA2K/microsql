#include "page.h"
#include "expected"
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

std::expected<uint8_t *, PageError> Page::updateRow(uint16_t slotIndex,
                                                     const uint8_t *rowBytes,
                                                     uint16_t rowLen) {
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
  }

  this->isDirty = true;
  return this->data + oldRowOffset;
}
