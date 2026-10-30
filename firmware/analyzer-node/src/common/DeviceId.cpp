#include "common/DeviceId.h"

#include <WiFi.h>

#include "ProjectConfig.h"

namespace usb_analyzer {

String macString() {
  uint8_t mac[6] = {};
  WiFi.macAddress(mac);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String makeDeviceId() {
  uint8_t mac[6] = {};
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s-%02x%02x", DEVICE_NAME_PREFIX, mac[4], mac[5]);
  return String(buf);
}

} // namespace usb_analyzer
