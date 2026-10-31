#pragma once

/**
 * Lab BCL 8-byte BE header (Desk FACT from technology-audit bmw-bcl / BclCarLink).
 * Claim: lab_bcl_on_aoa_bulk — NOT HU wire / NOT Etch/SAS.
 */

#include <cstdint>
#include <cstring>

namespace usb_analyzer {

enum class BclCmd : uint16_t {
  Open = 1,
  Data = 2,
  Close = 3,
  Handshake = 4,
  SelectProto = 5,
  DataAck = 6,
};

struct BclLabHeader {
  uint16_t command = 0;
  uint16_t src = 0;
  uint16_t dest = 0;
  uint16_t data_len = 0;
};

inline void bclPutBe16(uint8_t *p, uint16_t v) {
  p[0] = static_cast<uint8_t>((v >> 8) & 0xff);
  p[1] = static_cast<uint8_t>(v & 0xff);
}

inline uint16_t bclGetBe16(const uint8_t *p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

inline bool bclParseHeader(const uint8_t *buf, size_t n, BclLabHeader &out) {
  if (!buf || n < 8) {
    return false;
  }
  out.command = bclGetBe16(buf);
  out.src = bclGetBe16(buf + 2);
  out.dest = bclGetBe16(buf + 4);
  out.data_len = bclGetBe16(buf + 6);
  return true;
}

/** Pack header (+ optional payload) into out; returns total bytes or 0 on error. */
inline size_t bclPack(uint8_t *out, size_t out_cap, const BclLabHeader &h,
                      const uint8_t *payload, size_t payload_len) {
  if (!out || out_cap < 8u + payload_len) {
    return 0;
  }
  if (payload_len != h.data_len) {
    return 0;
  }
  bclPutBe16(out, h.command);
  bclPutBe16(out + 2, h.src);
  bclPutBe16(out + 4, h.dest);
  bclPutBe16(out + 6, h.data_len);
  if (payload_len && payload) {
    memcpy(out + 8, payload, payload_len);
  }
  return 8u + payload_len;
}

inline const char *bclCmdName(uint16_t cmd) {
  switch (cmd) {
  case 1:
    return "OPEN";
  case 2:
    return "DATA";
  case 3:
    return "CLOSE";
  case 4:
    return "HANDSHAKE";
  case 5:
    return "SELECTPROTO";
  case 6:
    return "DATAACK";
  default:
    return "CMD";
  }
}

/** Desk session-init magic (BclCarLink decodeHex("12345678")) — 4 bytes, not ASCII-8. */
inline constexpr uint8_t kBclLabInit[4] = {0x12, 0x34, 0x56, 0x78};

} // namespace usb_analyzer
