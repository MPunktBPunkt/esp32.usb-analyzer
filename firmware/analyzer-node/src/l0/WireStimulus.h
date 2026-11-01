#pragma once

#include <Arduino.h>

namespace usb_analyzer {

/**
 * Destructive OTG wire stimulus: claim GPIO19/20 after Host stack stopped,
 * drive controlled J/K/SE0 patterns toward the Device on the OTG cable.
 *
 * Claim: wire_stimulus_lab — NOT a USB Host, NOT Path A sniffer, NOT FS PHY tap.
 * Requires reboot to restore Host stack.
 */
class WireStimulus {
public:
  struct Result {
    bool ok = false;
    bool host_released = false;
    bool pins_claimed = false;
    String pattern;
    uint32_t rate_hz = 0;
    uint32_t achieved_rate_hz = 0;
    uint32_t states = 0;
    uint32_t duration_us = 0;
    uint32_t reboot_after_ms = 0;
    String note;
    String error;
  };

  /**
   * Host stack must already be stopped (UsbHostNode::releaseHostStack).
   * @param pattern bus_states|sync|eop|se0_reset|setup_ok|setup_bad_crc|data_bad_crc|glitch|jk_burst
   * @param rate_hz target line-state rate (default 100k; cap 2M)
   */
  static Result run(const String &pattern, uint32_t rate_hz,
                    uint32_t reboot_after_ms);
};

} // namespace usb_analyzer
