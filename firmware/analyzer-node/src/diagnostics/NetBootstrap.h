#pragma once

#include <Arduino.h>

namespace usb_analyzer {

struct NetConfig {
  String deviceName;
  String controllerHost;
  uint16_t controllerPort = 9750;
  String hubHost;
  uint16_t hubPort = 8093;
};

/** WiFiManager portal + NVS. No secrets in source. */
void netBootstrap(NetConfig &cfg);

/** Persist device name (e.g. after Hub rename). */
void netPersistDeviceName(const String &name);

void netMaintain();

} // namespace usb_analyzer
