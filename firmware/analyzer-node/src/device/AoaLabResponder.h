#pragma once

/**
 * Lab AOA EP0 responder (AOSP 51/52/53) + optional vendor-bulk echo.
 *
 * Control path: USBVendor.onRequest (TinyUSB CONTROL_STAGE_* — SETUP=1).
 * Bulk path: duplex echo on USBVendor when respond enabled (lab Rohr only).
 *
 * Claim: aoa_control_handshake_lab / aoa_bulk_echo_lab — NOT HU/BCL.
 */

#include <Arduino.h>
#include <cstdint>

class USBVendor;

namespace usb_analyzer {

class UsbDeviceNode;

class AoaLabResponder {
public:
  static AoaLabResponder &instance();

  void bind(UsbDeviceNode *device) { device_ = device; }
  void setVendor(USBVendor *v) { vendor_ = v; }
  USBVendor *vendor() const { return vendor_; }

  void setEnabled(bool on);
  bool enabled() const { return enabled_; }
  void setReenumOnStart(bool on) { reenum_on_start_ = on; }
  bool reenumOnStart() const { return reenum_on_start_; }
  void setBulkEcho(bool on) { bulk_echo_ = on; }
  bool bulkEcho() const { return bulk_echo_; }

  void resetCounters();
  uint32_t setupCount() const { return setup_count_; }
  uint32_t aoa51() const { return aoa51_; }
  uint32_t aoa52() const { return aoa52_; }
  uint32_t aoa53() const { return aoa53_; }
  uint32_t aoa51RespondOk() const { return aoa51_ok_; }
  uint32_t stringBytes() const { return string_bytes_; }
  uint32_t bulkRx() const { return bulk_rx_; }
  uint32_t bulkTx() const { return bulk_tx_; }
  uint8_t lastStringIndex() const { return last_string_index_; }
  const char *lastString() const { return last_string_; }

  void appendStatusJson(String &json) const;
  void pollBulkEcho();

  /** USBVendor.onRequest — USB task; stage is TinyUSB CONTROL_STAGE_*. */
  bool onVendorRequest(uint8_t rhport, uint8_t stage, const void *request);

  bool consumeStartPending();

private:
  AoaLabResponder() = default;
  UsbDeviceNode *device_ = nullptr;
  USBVendor *vendor_ = nullptr;
  bool enabled_ = false;
  bool reenum_on_start_ = true;
  bool bulk_echo_ = true;
  bool respond_in_flight_ = false;
  bool start_pending_ = false;
  uint16_t protocol_le_ = 1;
  uint32_t setup_count_ = 0;
  uint32_t aoa51_ = 0;
  uint32_t aoa52_ = 0;
  uint32_t aoa53_ = 0;
  uint32_t aoa51_ok_ = 0;
  uint32_t string_bytes_ = 0;
  uint32_t bulk_rx_ = 0;
  uint32_t bulk_tx_ = 0;
  uint8_t last_string_index_ = 0xff;
  char last_string_[48] = {0};
  uint8_t string_scratch_[64] = {0};
  uint16_t string_scratch_len_ = 0;
};

} // namespace usb_analyzer
