#include "diagnostics/NetBootstrap.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "ProjectConfig.h"
#include "common/DeviceId.h"

namespace usb_analyzer {
namespace {

Preferences prefs;
WiFiManager wm;
uint32_t last_button_ms = 0;
bool button_armed = false;

void checkResetButton() {
  const int pin = RESET_BUTTON_PIN;
  pinMode(pin, INPUT_PULLUP);
  if (digitalRead(pin) == LOW) {
    if (!button_armed) {
      button_armed = true;
      last_button_ms = millis();
    } else if (millis() - last_button_ms >=
               static_cast<uint32_t>(RESET_HOLD_SEC) * 1000UL) {
      Serial.println("[net] BOOT held — clearing WiFi and rebooting to portal");
      wm.resetSettings();
      delay(200);
      ESP.restart();
    }
  } else {
    button_armed = false;
  }
}

} // namespace

void netBootstrap(NetConfig &cfg) {
  checkResetButton();

  prefs.begin("usb-an", false);
  cfg.deviceName = prefs.getString("name", makeDeviceId());
  cfg.controllerHost =
      prefs.getString("ctrl_host", CONTROLLER_HOST_DEFAULT);
  cfg.controllerPort =
      prefs.getUShort("ctrl_port", CONTROLLER_PORT_DEFAULT);
  prefs.end();

  WiFi.mode(WIFI_STA);
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
  wm.setHostname(cfg.deviceName.c_str());

  WiFiManagerParameter p_name("name", "Device name", cfg.deviceName.c_str(), 40);
  WiFiManagerParameter p_host("ctrl_host", "Controller host",
                              cfg.controllerHost.c_str(), 40);
  char port_buf[8];
  snprintf(port_buf, sizeof(port_buf), "%u",
           static_cast<unsigned>(cfg.controllerPort));
  WiFiManagerParameter p_port("ctrl_port", "Controller port", port_buf, 6);
  wm.addParameter(&p_name);
  wm.addParameter(&p_host);
  wm.addParameter(&p_port);

  Serial.printf("[net] portal SSID %s (if no saved WiFi)\n", WIFI_AP_NAME);
  const bool ok = wm.autoConnect(WIFI_AP_NAME);
  if (!ok) {
    Serial.println("[net] WiFi portal failed/timeout — continuing offline for "
                   "USB Host bring-up");
  } else {
    Serial.printf("[net] WiFi OK ip=%s rssi=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }

  cfg.deviceName = p_name.getValue();
  cfg.controllerHost = p_host.getValue();
  cfg.controllerPort =
      static_cast<uint16_t>(atoi(p_port.getValue()) & 0xffff);
  if (cfg.deviceName.isEmpty()) {
    cfg.deviceName = makeDeviceId();
  }
  if (cfg.controllerHost.isEmpty()) {
    cfg.controllerHost = CONTROLLER_HOST_DEFAULT;
  }
  if (cfg.controllerPort == 0) {
    cfg.controllerPort = CONTROLLER_PORT_DEFAULT;
  }

  prefs.begin("usb-an", false);
  prefs.putString("name", cfg.deviceName);
  prefs.putString("ctrl_host", cfg.controllerHost);
  prefs.putUShort("ctrl_port", cfg.controllerPort);
  prefs.end();
}

void netMaintain() {
  checkResetButton();
  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t last = 0;
    if (millis() - last > 10000) {
      last = millis();
      Serial.println("[net] WiFi lost — reconnect");
      WiFi.reconnect();
    }
  }
}

} // namespace usb_analyzer
