#pragma once

#include <stdint.h>

namespace usb_analyzer {

/**
 * Shared Host↔Device lab pattern for OTG-loop MSC reads.
 * Device gadget fills sectors; Host verifies after READ(10).
 */
inline uint8_t labSectorByte(uint32_t lba, uint32_t offset_in_sector) {
  return static_cast<uint8_t>((lba * 131u + offset_in_sector * 17u + 0x5Au) & 0xFFu);
}

inline void fillLabSector(uint8_t *out, uint32_t len, uint32_t lba,
                          uint32_t offset_in_sector = 0) {
  for (uint32_t i = 0; i < len; i++) {
    out[i] = labSectorByte(lba, offset_in_sector + i);
  }
}

inline bool checkLabSector(const uint8_t *buf, uint32_t len, uint32_t lba,
                           uint32_t offset_in_sector = 0) {
  for (uint32_t i = 0; i < len; i++) {
    if (buf[i] != labSectorByte(lba, offset_in_sector + i)) {
      return false;
    }
  }
  return true;
}

} // namespace usb_analyzer
