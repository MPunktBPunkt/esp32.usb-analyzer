#pragma once

#include <Arduino.h>

namespace usb_analyzer {

/**
 * Path B GPIO receive on free pins (Device or Host) — cross-link sampler.
 * Claim: path_b_gpio_rx_lab — NOT OTG PHY sniffer.
 */
class PathBGpioRx {
public:
  static constexpr int kDefaultDpPin = 10;
  static constexpr int kDefaultDmPin = 11;
  static constexpr int kDefaultSyncPin = 12;
  static constexpr uint16_t kDefaultL0stPort = 9760;
  static constexpr uint32_t kMaxSamples = 2048;

  struct Result {
    bool ok = false;
    int dp_pin = kDefaultDpPin;
    int dm_pin = kDefaultDmPin;
    int sync_pin = kDefaultSyncPin;
    uint32_t rate_hz = 0;
    uint32_t achieved_rate_hz = 0;
    uint32_t samples = 0;
    uint32_t duration_us = 0;
    uint32_t edges = 0;
    uint32_t hist[4] = {0, 0, 0, 0}; // SE0 J K SE1 counts
    bool sync_seen = false;
    bool sync_timeout = false;
    bool l0st_ok = false;
    uint32_t l0st_bytes = 0;
    String mode;
    String note;
    String error;
  };

  /**
   * mode: rx | rx_sync | rx_l0st | rx_sync_l0st
   * If sync modes: wait rising edge on sync_pin then sample.
   */
  static Result run(const String &mode, uint32_t samples, uint32_t rate_hz,
                    int dp_pin, int dm_pin, int sync_pin, uint32_t timeout_ms,
                    const String &l0st_host, uint16_t l0st_port);
};

} // namespace usb_analyzer
