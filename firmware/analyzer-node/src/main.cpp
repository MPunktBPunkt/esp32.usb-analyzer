/**
 * esp32.usb-analyzer — analyzer-node
 * Stufe A: esp-hub core (DiagLog/WebUi/Ota/Hub) + Debian ControlClient + USB role
 */

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

#include <usb_analyzer/Role.h>

#include "BootContext.h"
#include "Checkpoint.h"
#include "CpuLoad.h"
#include "DiagLog.h"
#include "DiagSaved.h"
#include "HardwareInfo.h"
#include "HubClient.h"
#include "ModuleManager.h"
#include "OtaService.h"
#include "PanicTail.h"
#include "ProgressWatch.h"
#include "ProjectConfig.h"
#include "Recovery.h"
#include "WebUi.h"
#include "app/AnalyzerStatusModule.h"
#include "build_id.h"
#include "capture/CaptureLog.h"
#include "common/DeviceId.h"
#include "device/UsbDeviceNode.h"
#include "diagnostics/NetBootstrap.h"
#include "host/UsbHostNode.h"
#include "protocol/ControlClient.h"
#include "protocol/DataPlane.h"

using usb_analyzer::AnalyzerStatusModule;
using usb_analyzer::CaptureLog;
using usb_analyzer::ControlClient;
using usb_analyzer::DataPlane;
using usb_analyzer::NetConfig;
using usb_analyzer::Role;
using usb_analyzer::UsbDeviceNode;
using usb_analyzer::UsbHostNode;
using usb_analyzer::makeDeviceId;
using usb_analyzer::netBootstrap;
using usb_analyzer::netMaintain;
using usb_analyzer::netPersistDeviceName;
using usb_analyzer::roleName;

static UsbHostNode g_host;
static UsbDeviceNode g_device;
static ControlClient g_ctrl;
static DataPlane g_data;
static Role g_role = Role::Unassigned;

static HardwareInfo g_hw;
static HubClient g_hub;
static WebUi g_web;
static OtaService g_ota;
static ModuleManager g_modules;
static AnalyzerStatusModule g_status_mod;

static String g_device_name;
static String g_hub_host;
static int g_hub_port = HUB_PORT;
static unsigned long g_hb_interval_ms =
    static_cast<unsigned long>(DEFAULT_INTERVAL_S) * 1000UL;
static unsigned long g_last_hb_ms = 0;
static unsigned long g_last_success_ms = 0;
static bool g_ota_pending = false;
static String g_ota_url;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== esp32.usb-analyzer analyzer-node ===");
  Serial.printf("fw=%s git=%s\n", FW_VERSION, GIT_SHA);

  BootContext::begin();
  ProgressWatch::begin();
  CpuLoad::begin();

  g_hw.probe();
  g_hw.logToSerial();
  DiagSaved::persistBootSnapshot(&g_hw);
  DiagLog::persist("hw", false);

#if ANALYZER_DEFAULT_ROLE_HOST
  g_role = Role::Host;
#elif ANALYZER_DEFAULT_ROLE_DEVICE
  g_role = Role::Device;
#else
  g_role = Role::Unassigned;
#endif
  Serial.printf("default_role=%s\n", roleName(g_role));

  NetConfig net;
  Checkpoint::enter(Checkpoint::Wifi);
  netBootstrap(net);
  Checkpoint::leave(Checkpoint::Wifi);
  PanicTail::breadcrumb(PanicTail::WifiUp);
  ProgressWatch::note(ProgressWatch::Idle);

  if (net.deviceName.isEmpty()) {
    net.deviceName = makeDeviceId();
  }
  g_device_name = net.deviceName;
  g_hub_host = net.hubHost;
  g_hub_port = net.hubPort;
  Serial.printf("device_id=%s hub=%s:%d ctrl=%s:%u\n", g_device_name.c_str(),
                g_hub_host.c_str(), g_hub_port, net.controllerHost.c_str(),
                static_cast<unsigned>(net.controllerPort));

  g_ctrl.begin(g_device_name, net.controllerHost, net.controllerPort);
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

  g_status_mod.setRole(g_role);
  g_status_mod.setController(net.controllerHost, net.controllerPort);
  g_status_mod.setDataPlane(&g_data);
  g_status_mod.setUsbNodes(&g_host, &g_device);
  g_modules.add(&g_status_mod);
  g_data.setHostNode(&g_host);
  g_data.begin();
  CaptureLog::instance().begin(g_device_name.c_str(), roleName(g_role));

  g_hub.setHardware(&g_hw);
  g_hub.configure(g_device_name, g_hub_host, g_hub_port);
  g_web.setHardware(&g_hw);
  g_web.setOta(&g_ota);
  g_web.setModules(&g_modules);

  if (WiFi.status() == WL_CONNECTED) {
    String mdns = "usb-an-" + WiFi.macAddress();
    mdns.replace(":", "");
    mdns.toUpperCase();
    mdns = "usb-an-" + mdns.substring(6);
    if (MDNS.begin(mdns.c_str())) {
      Serial.println("[mDNS] " + mdns + ".local");
    }

    Checkpoint::enter(Checkpoint::Web);
    g_web.begin(g_device_name, g_hub_host, g_hub_port, g_hb_interval_ms);
    Checkpoint::leave(Checkpoint::Web);
    PanicTail::breadcrumb(PanicTail::WebUp);

    g_ota.setYield(+[]() { WebUi::pumpActiveClient(); });
    g_modules.beginAll();

    g_last_success_ms = millis();
    Checkpoint::enter(Checkpoint::Hub);
    bool name_changed = false;
    if (g_hub.sendHeartbeat(g_device_name, g_last_success_ms, g_hb_interval_ms,
                            g_ota_pending, g_ota_url, &name_changed)) {
      Checkpoint::leave(Checkpoint::Hub);
      PanicTail::breadcrumb(PanicTail::HubOk);
      ProgressWatch::note(ProgressWatch::Heartbeat);
      if (name_changed) {
        netPersistDeviceName(g_device_name);
        g_ctrl.setDeviceId(g_device_name);
        g_web.updateConfig(g_device_name, g_hub_host, g_hub_port,
                           g_hb_interval_ms);
      }
    } else {
      Checkpoint::leave(Checkpoint::Hub);
    }
    ProgressWatch::note(ProgressWatch::Idle);
    g_web.updateConfig(g_device_name, g_hub_host, g_hub_port, g_hb_interval_ms);
    g_last_hb_ms = millis();
  } else {
    Serial.println("[main] no WiFi — Hub/Web deferred (USB may still run)");
    g_modules.beginAll();
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
  g_status_mod.setRole(g_ctrl.role());
  g_data.loop();

  if (WiFi.status() == WL_CONNECTED) {
    g_web.handleClient();
    g_modules.loopAll();
    unsigned long now = millis();
    ProgressWatch::note(ProgressWatch::Idle);
    g_web.pushSseIfDue(now);

    if (now - g_last_hb_ms >= g_hb_interval_ms) {
      g_last_hb_ms = now;
      Checkpoint::enter(Checkpoint::Hub);
      bool name_changed = false;
      if (g_hub.sendHeartbeat(g_device_name, g_last_success_ms, g_hb_interval_ms,
                              g_ota_pending, g_ota_url, &name_changed)) {
        Checkpoint::leave(Checkpoint::Hub);
        PanicTail::breadcrumb(PanicTail::HubOk);
        ProgressWatch::note(ProgressWatch::Heartbeat);
        if (name_changed) {
          netPersistDeviceName(g_device_name);
          g_ctrl.setDeviceId(g_device_name);
        }
      } else {
        Checkpoint::leave(Checkpoint::Hub);
      }
      g_web.updateConfig(g_device_name, g_hub_host, g_hub_port, g_hb_interval_ms);
    }

    if (g_ota_pending) {
      g_ota_pending = false;
      Checkpoint::enter(Checkpoint::Ota);
      g_ota.scheduleUrl(g_ota_url);
      g_ota_url = "";
      g_ota.poll();
      Checkpoint::leave(Checkpoint::Ota);
    }

    now = millis();
    ProgressWatch::evaluate(now);

#if WATCHDOG_TIMEOUT_S > 0
    if (!g_ota.active() &&
        now - g_last_success_ms >
            static_cast<unsigned long>(WATCHDOG_TIMEOUT_S) * 1000UL) {
      Serial.println("[WDT] Neustart");
      DiagLog::printf("[WDT] legacy fire age_s=%lu\n",
                      static_cast<unsigned long>((now - g_last_success_ms) /
                                                 1000UL));
      PanicTail::breadcrumb(PanicTail::WdtFire);
      Recovery::requestRestart(Recovery::LegacyWatchdog, "heartbeat_timeout");
    }
#endif
  }

  CpuLoad::poll();
  // Tighter loop while data-plane peer is active (Gate 3 integrity / later proxy).
  delay(strcmp(g_data.state(), "listen") == 0 ||
                strcmp(g_data.state(), "offline") == 0 ||
                strcmp(g_data.state(), "no_wifi") == 0
            ? 5
            : 1);
}
