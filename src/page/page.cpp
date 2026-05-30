#include "page.h"
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

int Page::insertRow(const uint8_t *rowBytes, uint16_t rowLen) {
  uint16_t slots = this->numSlots();
  uint16_t freeptr = this->read16(HEADER_OFFSET_FREE_PTR);
  int slotDirEnd = HEADER_SIZE + (slots * SLOT_ENTRY_SIZE);

  if (this->freeSpace() < rowLen + SLOT_ENTRY_SIZE) {
    return -1;
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

uint8_t *Page::readRow(uint16_t slotIndex, uint16_t *rowLen) {
  if (slotIndex >= this->numSlots()) {
    return nullptr;
  }
  if (this->slotDeleted(slotIndex)) {
    return nullptr;
  }

  int slot = HEADER_SIZE + slotIndex * SLOT_ENTRY_SIZE;
  uint16_t rowOffset = this->read16(slot);
  *rowLen = this->read16(slot + 2);

  return this->data + rowOffset;
}

uint8_t *Page::deleteRow(uint16_t slotIndex) {
  if (slotIndex >= this->numSlots()) {
    return nullptr;
  }
  if (this->slotDeleted(slotIndex)) {
    return nullptr;
  }

  int slot = HEADER_SIZE + slotIndex * SLOT_ENTRY_SIZE;
  uint16_t rowOffset = this->read16(slot);

  this->write16(slot + 2, 0);

  return this->data + rowOffset;
}

uint8_t *Page::updateRow(uint16_t slotIndex, const uint8_t *rowBytes,
                         uint16_t rowLen) {
  if (slotIndex >= this->numSlots()) {
    return nullptr;
  }

  int slot = HEADER_SIZE + slotIndex * SLOT_ENTRY_SIZE;
  uint16_t oldRowOffset = this->read16(slot);
  uint16_t oldRowLen = this->read16(slot + 2);

  if (rowLen == 0) {
    return nullptr;
  }
  if (oldRowLen != rowLen) {
    return nullptr;
  }
  std::memcpy(data + oldRowOffset, rowBytes, rowLen);
  this->isDirty = true;

  return this->data + oldRowOffset;
}
