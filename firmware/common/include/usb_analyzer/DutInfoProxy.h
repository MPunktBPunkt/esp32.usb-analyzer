#pragma once

#include <stdint.h>

namespace usb_analyzer {

/**
 * Gate 4 enum-forward slice — DUT snapshot over DataPlane (not USB control MITM).
 * Host fills from EspUsbHost + optional MSC inquiry/capacity.
 */
#pragma pack(push, 1)
struct DutInfoSnapshot {
  uint8_t present; // 0/1
  uint8_t address;
  uint8_t speed; // 0 unk, 1 low, 2 full, 3 high
  uint8_t msc_ready;
  uint16_t vid;
  uint16_t pid;
  uint16_t block_size;
  uint16_t reserved;
  uint32_t capacity_blocks;
  char mfr[20];
  char product[36];
  char serial[20];
  char inq_vendor[8];
  char inq_product[16];
  char inq_rev[4];
};
#pragma pack(pop)

// 4+8+4 +20+36+20 +8+16+4 = 120 — pad to 128
static_assert(sizeof(DutInfoSnapshot) == 120, "DutInfoSnapshot size");

} // namespace usb_analyzer
