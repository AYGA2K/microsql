#include <cstdint>
#include <fstream>
#include <string>
inline constexpr int PAGE_SIZE = 4096;
inline constexpr int HEADER_SIZE = 16;
inline constexpr int SLOT_ENTRY_SIZE = 4;
inline constexpr int HEADER_OFFSET_PAGE_ID = 0;   // 4 bytes
inline constexpr int HEADER_OFFSET_NUM_SLOTS = 4; // 2 bytes
inline constexpr int HEADER_OFFSET_FREE_PTR = 6;  // 2 bytes
inline constexpr int HEADER_OFFSET_FLAGS = 8;     // 4 bytes

// Page header fields:
// The first 16 bytes of data are the header:
// pageId at offset 0, 4 bytes
// numSlots at offset 4, 2 bytes
// freeSpacePtr at offset 6, 2 bytes — the byte offset within the page where the
// next row will be written (starts at 4096, moves down as rows are added)
// flags at offset 8, 4 bytes
// reserved at offset 12, 4 bytes

struct Page {
  uint8_t data[PAGE_SIZE];
  uint32_t pageId;
  bool isDirty; // whether this page has been modified since it was last written
  // to disk.

  uint16_t read16(size_t offset) const;
  void write16(size_t offset, uint16_t value);
  uint16_t freeSpace() const;
  uint16_t numSlots() const;
  bool slotDeleted(uint16_t slotIndex) const;
  void init(uint32_t id);
  int insertRow(const uint8_t *rowBytes, uint16_t rowLen);
  uint8_t *readRow(uint16_t slotIndex, uint16_t *rowLen);
  uint8_t *deleteRow(uint16_t slotIndex);
  uint8_t *updateRow(uint16_t slotIndex, const uint8_t *rowBytes, uint16_t rowLen);
};

struct TableFile {
  std::string filePath;
  std::fstream file;

  bool open(const std::string &path);
  void close();
  void readPage(uint32_t pageId, Page &p);
  void writePage(const Page &p);
  uint32_t allocatePage();
  int numPages();
};
