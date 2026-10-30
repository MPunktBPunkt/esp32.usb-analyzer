/**
 * esp32.usb-analyzer — analyzer-node
 * Gate 1: USB Host · Gate 2: USB Device (build env selects defaults)
 */

#include <Arduino.h>
#include <WiFi.h>

#include <usb_analyzer/Role.h>

#include "ProjectConfig.h"
#include "build_id.h"
#include "common/DeviceId.h"
#include "device/UsbDeviceNode.h"
#include "diagnostics/NetBootstrap.h"
#include "host/UsbHostNode.h"
#include "protocol/ControlClient.h"

using usb_analyzer::ControlClient;
using usb_analyzer::NetConfig;
using usb_analyzer::Role;
using usb_analyzer::UsbDeviceNode;
using usb_analyzer::UsbHostNode;
using usb_analyzer::makeDeviceId;
using usb_analyzer::netBootstrap;
using usb_analyzer::netMaintain;
using usb_analyzer::roleName;

static UsbHostNode g_host;
static UsbDeviceNode g_device;
static ControlClient g_ctrl;
static Role g_role = Role::Unassigned;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("esp32.usb-analyzer analyzer-node");
  Serial.printf("fw=%s git=%s\n", FW_VERSION, GIT_SHA);
  Serial.printf("psram=%u free_psram=%u\n",
                static_cast<unsigned>(ESP.getPsramSize()),
                static_cast<unsigned>(ESP.getFreePsram()));

#if ANALYZER_DEFAULT_ROLE_HOST
  g_role = Role::Host;
#elif ANALYZER_DEFAULT_ROLE_DEVICE
  g_role = Role::Device;
#else
  g_role = Role::Unassigned;
#endif
  Serial.printf("default_role=%s (runtime assignable via controller)\n",
                roleName(g_role));

  NetConfig net;
  netBootstrap(net);
  if (net.deviceName.isEmpty()) {
    net.deviceName = makeDeviceId();
  }
  Serial.printf("device_id=%s\n", net.deviceName.c_str());

  g_ctrl.begin(net.deviceName, net.controllerHost, net.controllerPort);
  g_ctrl.setRole(g_role);
  g_ctrl.setHostNode(&g_host);

  if (g_role == Role::Host) {
#if ANALYZER_ENABLE_USB_HOST
    if (!g_host.begin()) {
      Serial.println("[main] USB Host begin failed — see logs");
    }
#else
    Serial.println("[main] host role but ANALYZER_ENABLE_USB_HOST=0");
#endif
  } else if (g_role == Role::Device) {
#if ANALYZER_ENABLE_USB_DEVICE
    if (!g_device.begin()) {
      Serial.println("[main] USB Device begin failed — see logs");
    }
#else
    Serial.println("[main] device role but ANALYZER_ENABLE_USB_DEVICE=0");
#endif
  } else {
    Serial.println("[main] unassigned — waiting for controller role");
  }

  Serial.println("[main] setup done");
}

void loop() {
  netMaintain();
  if (g_role == Role::Host) {
    g_host.loop();
  } else if (g_role == Role::Device) {
    g_device.loop();
  }
  g_ctrl.loop();
  delay(5);
}
