#pragma once

#include <Arduino.h>

namespace usb_analyzer {

/**
 * Path B lab substitute: bit-bang FS line-states on free GPIOs (NOT 19/20),
 * optionally sample them back and/or push L0ST to Debian ingest.
 * Claim: path_b_gpio_lab — NOT live OTG D+/D− PHY capture.
 */
class PathBGpioLab {
public:
  static constexpr int kDefaultDpPin = 10; // simulated D+
  static constexpr int kDefaultDmPin = 11; // simulated D−
  static constexpr uint16_t kDefaultL0stPort = 9760;

  struct Result {
    bool ok = false;
    int dp_pin = kDefaultDpPin;
    int dm_pin = kDefaultDmPin;
    uint32_t rate_hz = 0;
    uint32_t states_tx = 0;
    uint32_t states_rx = 0;
    uint32_t mismatches = 0;
    uint32_t duration_us = 0;
    uint32_t achieved_rate_hz = 0;
    bool loopback_ok = false;
    bool l0st_ok = false;
    uint32_t l0st_bytes = 0;
    String l0st_host;
    uint16_t l0st_port = kDefaultL0stPort;
    String mode;
    String note;
    String error;
  };

  /** tx | txrx | txrx_l0st */
  Result run(const String &mode, uint32_t rate_hz, int dp_pin, int dm_pin,
             const String &l0st_host, uint16_t l0st_port);
};

} // namespace usb_analyzer
