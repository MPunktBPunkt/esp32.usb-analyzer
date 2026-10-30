#pragma once

#include <Arduino.h>

namespace usb_analyzer {

struct NetConfig {
  String deviceName;
  String controllerHost;
  uint16_t controllerPort = 9750;
};

/** WiFiManager portal + NVS. No secrets in source. */
void netBootstrap(NetConfig &cfg);

void netMaintain();

} // namespace usb_analyzer
