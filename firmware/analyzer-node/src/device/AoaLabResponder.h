#pragma once

/**
 * Lab AOA EP0 responder (AOSP 51/52/53) for OTG Host↔Device loop.
 *
 * Desk FACT from technology-audit usb-aoa + hu-probe C1b lab:
 * - Stage uses TinyUSB CONTROL_STAGE_* (SETUP=1), not Arduino REQUEST_STAGE_*
 * - After SETUP+tud_control_xfer, DATA/ACK complete must return true or EP0 stalls
 *
 * Claim: aoa_control_handshake_lab — NOT HU wire, NOT BCL, NOT accessory bulk IF.
 */

#include <Arduino.h>
#include <cstdint>

namespace usb_analyzer {

class UsbDeviceNode;

class AoaLabResponder {
public:
  static AoaLabResponder &instance();

  void bind(UsbDeviceNode *device) { device_ = device; }
  void setEnabled(bool on);
  bool enabled() const { return enabled_; }
  /** When true, START(53) soft-reenums gadget as 18D1:2D00 (MSC IF remains). */
  void setReenumOnStart(bool on) { reenum_on_start_ = on; }
  bool reenumOnStart() const { return reenum_on_start_; }

  void resetCounters();
  uint32_t setupCount() const { return setup_count_; }
  uint32_t aoa51() const { return aoa51_; }
  uint32_t aoa52() const { return aoa52_; }
  uint32_t aoa53() const { return aoa53_; }
  uint32_t aoa51RespondOk() const { return aoa51_ok_; }
  uint32_t stringBytes() const { return string_bytes_; }
  uint8_t lastStringIndex() const { return last_string_index_; }
  /** Truncated last captured SEND_STRING payload (UTF-8). */
  const char *lastString() const { return last_string_; }

  void appendStatusJson(String &json) const;

  /**
   * TinyUSB vendor control hook (via tinyusb_vendor_control_request_cb).
   * USB task context — no HTTP / long work.
   */
  bool onControl(uint8_t rhport, uint8_t stage, const void *request);

  /** Called from UsbDeviceNode::loop after START schedules re-enum. */
  bool consumeStartPending();

private:
  AoaLabResponder() = default;
  UsbDeviceNode *device_ = nullptr;
  bool enabled_ = false;
  bool reenum_on_start_ = true;
  bool respond_in_flight_ = false;
  bool start_pending_ = false;
  uint16_t protocol_le_ = 1; // AOA 1.0
  uint32_t setup_count_ = 0;
  uint32_t aoa51_ = 0;
  uint32_t aoa52_ = 0;
  uint32_t aoa53_ = 0;
  uint32_t aoa51_ok_ = 0;
  uint32_t string_bytes_ = 0;
  uint8_t last_string_index_ = 0xff;
  char last_string_[48] = {0};
  uint8_t string_scratch_[64] = {0};
  uint16_t string_scratch_len_ = 0;
};

} // namespace usb_analyzer
