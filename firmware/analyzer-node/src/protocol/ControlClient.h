#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include <usb_analyzer/Role.h>

namespace usb_analyzer {

class UsbHostNode;

/**
 * Minimal Gate 1 control client: TCP NDJSON HELLO to Debian discovery.
 * Does not yet implement full session assign / peer tunnel.
 */
class ControlClient {
public:
  void begin(const String &deviceId, const String &controllerHost, uint16_t port);
  void setRole(Role role) { role_ = role; }
  Role role() const { return role_; }
  void setHostNode(UsbHostNode *node) { host_node_ = node; }
  void loop();
  bool connected();

private:
  WiFiClient client_;
  String device_id_;
  String controller_host_;
  uint16_t port_ = 9750;
  Role role_ = Role::Unassigned;
  UsbHostNode *host_node_ = nullptr;
  uint32_t last_hello_ms_ = 0;
  uint32_t last_connect_attempt_ms_ = 0;

  void ensureConnected();
  void sendHello();
};

} // namespace usb_analyzer
