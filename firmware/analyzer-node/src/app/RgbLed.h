#pragma once

#include <Arduino.h>

namespace usb_analyzer {

/** Onboard WS2812 RGB helper (identify blink + solid color). */
class RgbLed {
public:
  void begin(uint8_t pin);
  void loop();

  void off();
  void setRgb(uint8_t r, uint8_t g, uint8_t b);
  /** Named: off|red|green|blue|cyan|magenta|yellow|white. Returns false if unknown. */
  bool setColorName(const String &name);
  /** Blink bright white for durationMs, then restore previous solid color. */
  void identify(uint32_t durationMs = 5000);

  const char *colorName() const { return color_name_; }
  bool identifying() const { return identify_until_ms_ != 0; }

  void appendStatusJson(String &json) const;

private:
  void writeRaw(uint8_t r, uint8_t g, uint8_t b);

  uint8_t pin_ = 48;
  bool ready_ = false;
  uint8_t r_ = 0, g_ = 0, b_ = 0;
  const char *color_name_ = "off";
  uint32_t identify_until_ms_ = 0;
  uint32_t identify_last_toggle_ms_ = 0;
  bool identify_on_ = false;
};

} // namespace usb_analyzer
