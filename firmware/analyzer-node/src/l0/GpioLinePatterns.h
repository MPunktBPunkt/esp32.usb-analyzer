#pragma once

#include <Arduino.h>
#include <vector>

namespace usb_analyzer {

/** Shared lab line-state patterns (SE0=0 J=1 K=2 SE1=3). */
std::vector<uint8_t> buildGpioLinePattern(const String &pattern, String *err);

/** gap_<n> idle J states between two SYNC bursts; glitch_se0_<n>; glitch_k_<n> */
bool isParametricPattern(const String &pattern);

} // namespace usb_analyzer
