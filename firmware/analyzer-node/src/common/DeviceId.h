#pragma once

#include <Arduino.h>

namespace usb_analyzer {

/** Stable id: usb-analyzer-<last 4 mac hex>. */
String makeDeviceId();

/** Full STA MAC as aa:bb:cc:dd:ee:ff (lowercase). */
String macString();

} // namespace usb_analyzer
