#include "app/RgbLed.h"

#if defined(ESP32)
#include <esp32-hal-rgb-led.h>
#endif

namespace usb_analyzer {

void RgbLed::begin(uint8_t pin) {
  pin_ = pin;
  ready_ = true;
  off();
  Serial.printf("[led] RGB WS2812 on GPIO%u\n", static_cast<unsigned>(pin_));
}

void RgbLed::writeRaw(uint8_t r, uint8_t g, uint8_t b) {
  if (!ready_) {
    return;
  }
#if defined(ESP32)
  neopixelWrite(pin_, r, g, b);
#else
  (void)r;
  (void)g;
  (void)b;
#endif
}

void RgbLed::off() {
  identify_until_ms_ = 0;
  r_ = g_ = b_ = 0;
  color_name_ = "off";
  writeRaw(0, 0, 0);
}

void RgbLed::setRgb(uint8_t r, uint8_t g, uint8_t b) {
  identify_until_ms_ = 0;
  r_ = r;
  g_ = g;
  b_ = b;
  if (r == 0 && g == 0 && b == 0) {
    color_name_ = "off";
  } else if (r && !g && !b) {
    color_name_ = "red";
  } else if (!r && g && !b) {
    color_name_ = "green";
  } else if (!r && !g && b) {
    color_name_ = "blue";
  } else if (!r && g && b) {
    color_name_ = "cyan";
  } else if (r && !g && b) {
    color_name_ = "magenta";
  } else if (r && g && !b) {
    color_name_ = "yellow";
  } else if (r && g && b) {
    color_name_ = "white";
  } else {
    color_name_ = "custom";
  }
  writeRaw(r_, g_, b_);
}

bool RgbLed::setColorName(const String &name) {
  String n = name;
  n.toLowerCase();
  n.trim();
  if (n == "off" || n == "0" || n == "black") {
    off();
    return true;
  }
  if (n == "red") {
    setRgb(64, 0, 0);
    color_name_ = "red";
    return true;
  }
  if (n == "green") {
    setRgb(0, 64, 0);
    color_name_ = "green";
    return true;
  }
  if (n == "blue") {
    setRgb(0, 0, 64);
    color_name_ = "blue";
    return true;
  }
  if (n == "cyan") {
    setRgb(0, 48, 48);
    color_name_ = "cyan";
    return true;
  }
  if (n == "magenta" || n == "pink") {
    setRgb(64, 0, 48);
    color_name_ = "magenta";
    return true;
  }
  if (n == "yellow") {
    setRgb(48, 48, 0);
    color_name_ = "yellow";
    return true;
  }
  if (n == "white") {
    setRgb(40, 40, 40);
    color_name_ = "white";
    return true;
  }
  return false;
}

void RgbLed::identify(uint32_t durationMs) {
  if (durationMs < 500) {
    durationMs = 500;
  }
  if (durationMs > 30000) {
    durationMs = 30000;
  }
  identify_until_ms_ = millis() + durationMs;
  identify_last_toggle_ms_ = 0;
  identify_on_ = false;
}

void RgbLed::loop() {
  if (!identify_until_ms_) {
    return;
  }
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - identify_until_ms_) >= 0) {
    identify_until_ms_ = 0;
    writeRaw(r_, g_, b_);
    return;
  }
  if (identify_last_toggle_ms_ == 0 ||
      (now - identify_last_toggle_ms_) >= 200) {
    identify_last_toggle_ms_ = now;
    identify_on_ = !identify_on_;
    if (identify_on_) {
      writeRaw(80, 80, 80);
    } else {
      writeRaw(0, 0, 0);
    }
  }
}

void RgbLed::appendStatusJson(String &json) const {
  json += ",\"led\":{\"color\":\"";
  json += color_name_;
  json += "\",\"identify\":";
  json += identifying() ? "true" : "false";
  json += ",\"pin\":";
  json += String(static_cast<unsigned>(pin_));
  json += "}";
}

} // namespace usb_analyzer
