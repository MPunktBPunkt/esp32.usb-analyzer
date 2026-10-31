/**
 * Gate-6: TinyUSB class-driver CBW/CSW hooks (vendored msc_device.c).
 * source: class_hook_mscd_xfer — not App fiction, not D+/D− wire.
 */

#include "ProjectConfig.h"

#if ANALYZER_ENABLE_USB_DEVICE

#include "AnalyzerMscHooks.h"
#include "capture/CaptureLog.h"

#include <Arduino.h>
#include <cstring>

namespace {

uint32_t rd_be32(const uint8_t *p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) |
         static_cast<uint32_t>(p[3]);
}

uint16_t rd_be16(const uint8_t *p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                               static_cast<uint16_t>(p[1]));
}

} // namespace

extern "C" void analyzer_msc_on_cbw(const void *cbw, uint32_t len) {
  if (!cbw || len < 31) {
    return;
  }
  const uint8_t *b = static_cast<const uint8_t *>(cbw);
  // BOT CBW: sig[0..3] tag[4..7] total[8..11] flags[12] lun[13] cblen[14] cdb[15..]
  const uint8_t op = b[15];
  uint32_t lba = 0;
  uint32_t blocks = 0;
  if (op == 0x28 || op == 0x2A) {
    lba = rd_be32(b + 15 + 2);     // CDB bytes 2..5
    blocks = rd_be16(b + 15 + 7);  // CDB bytes 7..8
  }
  usb_analyzer::CaptureLog::instance().pushPayload(
      usb_analyzer::CapRole::Device, usb_analyzer::CapType::BotCbw, 0, len, lba,
      blocks, 0, "class_hook_cbw", b, static_cast<uint8_t>(len > 31 ? 31 : len));
}

extern "C" void analyzer_msc_on_csw(const void *csw, uint32_t len) {
  if (!csw || len < 13) {
    return;
  }
  const uint8_t *b = static_cast<const uint8_t *>(csw);
  // CSW: sig[0..3] tag[4..7] residue[8..11] status[12]
  const uint32_t residue = static_cast<uint32_t>(b[8]) |
                           (static_cast<uint32_t>(b[9]) << 8) |
                           (static_cast<uint32_t>(b[10]) << 16) |
                           (static_cast<uint32_t>(b[11]) << 24);
  const uint8_t status = b[12];
  usb_analyzer::CaptureLog::instance().pushPayload(
      usb_analyzer::CapRole::Device, usb_analyzer::CapType::BotCsw, status, len,
      status, residue, 0, "class_hook_csw", b,
      static_cast<uint8_t>(len > 13 ? 13 : len));
}

#endif // ANALYZER_ENABLE_USB_DEVICE
