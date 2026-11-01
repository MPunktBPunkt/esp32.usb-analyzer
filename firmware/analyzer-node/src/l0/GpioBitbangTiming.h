#pragma once

#include <Arduino.h>
#include <esp_cpu.h>
#include <esp_timer.h>

namespace usb_analyzer {

/** Cycle-count busy-wait calibrated against esp_timer (lab GPIO bit-bang). */
class GpioBitbangTiming {
public:
  struct CalResult {
    bool ok = false;
    uint32_t cpu_mhz = 0;
    uint32_t cycles_per_us = 0;
    uint32_t measure_us = 0;
    uint32_t measured_cycles = 0;
    uint32_t err_ppm = 0; // |meas-expect|/expect
    String note;
  };

  static CalResult calibrate(uint32_t spin_us = 1000);

  /** Busy-wait approximately us microseconds using calibrated cycles. */
  static void waitUs(uint32_t us);

  static uint32_t cyclesPerUs() { return cycles_per_us_; }
  static bool calibrated() { return calibrated_; }

private:
  static bool calibrated_;
  static uint32_t cycles_per_us_;
};

} // namespace usb_analyzer
