#pragma once

#include <Arduino.h>

namespace usb_analyzer {

/**
 * Gate 1: start EspUsbHost and log enumeration (VID/PID/config/interfaces).
 *
 * Does NOT claim MITM, capture integrity, or wire-level visibility.
 */
class UsbHostNode {
public:
  bool begin();
  void loop();
  bool ready() const { return ready_; }
  bool beginOk() const { return begin_ok_; }
  size_t deviceCount() const;
  String lastEventJson() const { return last_event_json_; }

private:
  bool ready_ = false;
  bool begin_ok_ = false;
  bool hint_printed_ = false;
  uint32_t started_ms_ = 0;
  uint32_t last_report_ms_ = 0;
  String last_event_json_;

  void printNoDeviceHints() const;
};

} // namespace usb_analyzer
