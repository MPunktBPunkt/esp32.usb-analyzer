#include "common/DeviceId.h"

#include <esp_mac.h>

#include "ProjectConfig.h"

namespace usb_analyzer {
namespace {

bool readStaMac(uint8_t mac[6]) {
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    return true;
  }
  const uint64_t efuse = ESP.getEfuseMac();
  mac[0] = static_cast<uint8_t>(efuse >> 0);
  mac[1] = static_cast<uint8_t>(efuse >> 8);
  mac[2] = static_cast<uint8_t>(efuse >> 16);
  mac[3] = static_cast<uint8_t>(efuse >> 24);
  mac[4] = static_cast<uint8_t>(efuse >> 32);
  mac[5] = static_cast<uint8_t>(efuse >> 40);
  return true;
}

} // namespace

String macString() {
  uint8_t mac[6] = {};
  readStaMac(mac);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String makeDeviceId() {
  uint8_t mac[6] = {};
  readStaMac(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s-%02x%02x", DEVICE_NAME_PREFIX, mac[4], mac[5]);
  return String(buf);
}

} // namespace usb_analyzer
