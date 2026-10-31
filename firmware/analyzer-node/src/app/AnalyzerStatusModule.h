#pragma once

#include "BaseModule.h"
#include "app/RgbLed.h"
#include <usb_analyzer/Role.h>

namespace usb_analyzer {

class DataPlane;
class UsbHostNode;
class UsbDeviceNode;

/** Additive status + lab APIs (integrity / led / usb-host-probe) */
class AnalyzerStatusModule : public BaseModule {
public:
  const char *name() const override { return "analyzer"; }
  void begin() override;
  void loop() override;
  void setRole(Role role) { role_ = role; }
  void setController(const String &host, uint16_t port);
  void setDataPlane(DataPlane *dp) { data_ = dp; }
  void setUsbNodes(UsbHostNode *host, UsbDeviceNode *device) {
    host_ = host;
    device_ = device;
  }
  void appendStatusJson(String &json) override;
  void registerRoutes(WebServer &server) override;

private:
  Role role_ = Role::Unassigned;
  String controller_host_;
  uint16_t controller_port_ = 9750;
  DataPlane *data_ = nullptr;
  UsbHostNode *host_ = nullptr;
  UsbDeviceNode *device_ = nullptr;
  WebServer *server_ = nullptr;
  RgbLed led_;

  void handleIntegrity();
  void handleLed();
  void handleUsbHostProbe();
  void handleUsbProxy();
  void handleCapture();
};

} // namespace usb_analyzer
