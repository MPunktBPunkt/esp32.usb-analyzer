#pragma once

#include <Arduino.h>

namespace usb_analyzer {

/**
 * Gate 2: USB Device (gadget) bring-up.
 * Claim depth: enumeration / mount epochs only — not MITM.
 */
class UsbDeviceNode {
public:
  bool begin();
  void loop();
  bool ready() const { return ready_; }
  bool beginOk() const { return begin_ok_; }

private:
  bool ready_ = false;
  bool begin_ok_ = false;
};

} // namespace usb_analyzer
