#pragma once

#include <Arduino.h>
#include <driver/gpio.h>

namespace usb_analyzer {

/** Lab sync edge on a free GPIO (default 12) — NOT USB pins 19/20. */
class GpioSyncMarker {
public:
  static constexpr int kDefaultPin = 12;

  static void pulseOut(int pin, uint32_t high_us = 50);
  static bool waitEdge(int pin, uint32_t timeout_ms, bool rising = true);
};

} // namespace usb_analyzer
