/**
 * Lab access to private EspUsbHost::mscCommand for MODE_SENSE6 / PREVENT_ALLOW.
 * Field FACT (hu-probe 2026-10-19 P-MOUNT): MODE_SENSE6×6, PREVENT_ALLOW×7.
 * Claim: bmw_hu_stimulus_lab — NOT real HU wire.
 */
#include "host/MscScsiLab.h"

#if ANALYZER_ENABLE_USB_HOST

// EspUsbHost keeps mscCommand/findMscDevice private — lab unlock for SCSI extras.
#define private public
#include <EspUsbHost.h>
#undef private

#include <cstring>

namespace usb_analyzer {
namespace {

constexpr uint8_t kScsiModeSense6 = 0x1A;
constexpr uint8_t kScsiPreventAllow = 0x1E;

} // namespace

bool mscLabModeSense6(EspUsbHost &usb, uint8_t page_code, uint8_t alloc_len,
                      uint8_t *out, size_t out_len, uint32_t timeout_ms) {
  EspUsbHost::DeviceState *device = usb.findMscDevice(ESP_USB_HOST_ANY_ADDRESS);
  if (!device) {
    return false;
  }
  uint8_t cmd[6] = {};
  cmd[0] = kScsiModeSense6;
  cmd[2] = page_code;
  cmd[4] = alloc_len;
  uint8_t stack[192];
  uint8_t *buf = out;
  size_t n = out_len;
  if (!buf || n == 0) {
    buf = stack;
    n = alloc_len ? alloc_len : sizeof(stack);
    if (n > sizeof(stack)) {
      n = sizeof(stack);
    }
  }
  memset(buf, 0, n);
  return usb.mscCommand(*device, cmd, sizeof(cmd), buf, n, true, timeout_ms);
}

bool mscLabPreventAllow(EspUsbHost &usb, bool prevent, uint32_t timeout_ms) {
  EspUsbHost::DeviceState *device = usb.findMscDevice(ESP_USB_HOST_ANY_ADDRESS);
  if (!device) {
    return false;
  }
  uint8_t cmd[6] = {};
  cmd[0] = kScsiPreventAllow;
  cmd[4] = prevent ? 0x01 : 0x00;
  return usb.mscCommand(*device, cmd, sizeof(cmd), nullptr, 0, false, timeout_ms);
}

} // namespace usb_analyzer

#endif
