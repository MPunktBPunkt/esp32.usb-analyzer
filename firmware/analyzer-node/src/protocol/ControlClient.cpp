#include "protocol/ControlClient.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "ProjectConfig.h"
#include "build_id.h"
#include "common/DeviceId.h"
#include "host/UsbHostNode.h"

namespace usb_analyzer {

void ControlClient::begin(const String &deviceId, const String &controllerHost,
                          uint16_t port) {
  device_id_ = deviceId;
  controller_host_ = controllerHost;
  port_ = port;
  Serial.printf("[ctrl] target %s:%u\n", controller_host_.c_str(),
                static_cast<unsigned>(port_));
}

bool ControlClient::connected() { return client_.connected(); }

void ControlClient::ensureConnected() {
  if (client_.connected()) {
    return;
  }
  const uint32_t now = millis();
  if (now - last_connect_attempt_ms_ < 3000) {
    return;
  }
  last_connect_attempt_ms_ = now;
  Serial.printf("[ctrl] connecting %s:%u ...\n", controller_host_.c_str(),
                static_cast<unsigned>(port_));
  if (client_.connect(controller_host_.c_str(), port_, 2000)) {
    Serial.println("[ctrl] connected");
    last_hello_ms_ = 0; // send immediately
  } else {
    Serial.println("[ctrl] connect failed (controller optional for Gate 1 USB)");
  }
}

void ControlClient::sendHello() {
  if (!client_.connected()) {
    return;
  }

  JsonDocument body;
  body["device_id"] = device_id_;
  body["mac"] = macString();
  body["ip"] = WiFi.localIP().toString();
  body["fw"] = FW_VERSION;
  body["git_sha"] = GIT_SHA;
  JsonObject hw = body["hw"].to<JsonObject>();
  hw["chip"] = ESP.getChipModel();
  hw["rev"] = String(ESP.getChipRevision());
  hw["board"] = "esp32-s3-devkitc-1";
  JsonObject usb = body["usb"].to<JsonObject>();
  usb["controller"] = "otg-fs";
  usb["phy"] = "internal";
  JsonArray caps_role = usb["role_capability"].to<JsonArray>();
  caps_role.add("host");
  caps_role.add("device");
  JsonArray caps = body["capabilities"].to<JsonArray>();
  caps.add("usb-host");
  caps.add("usb-device");
  caps.add("capture");
  caps.add("tcp");
  if (ESP.getPsramSize() > 0) {
    caps.add("psram");
  }
  body["role"] = roleName(role_);
  body["uptime_s"] = static_cast<uint32_t>(millis() / 1000UL);
  body["free_heap"] = ESP.getFreeHeap();
  body["free_psram"] = ESP.getFreePsram();
  body["psram_size"] = ESP.getPsramSize();
  body["wifi_rssi"] = WiFi.RSSI();
  body["tcp_state"] =
      client_.connected() ? "control_connected" : "control_disconnected";
  if (host_node_) {
    body["usb_devices"] = static_cast<uint32_t>(host_node_->deviceCount());
    if (host_node_->lastEventJson().length() > 0) {
      JsonDocument ev;
      if (deserializeJson(ev, host_node_->lastEventJson()) ==
          DeserializationError::Ok) {
        body["last_usb_event"] = ev.as<JsonObject>();
      }
    }
  }

  JsonDocument env;
  env["v"] = 1;
  env["msg"] = "hello";
  env["ts_us"] = static_cast<uint64_t>(esp_timer_get_time());
  env["node_id"] = device_id_;
  env["session_id"] = nullptr;
  env["body"] = body;

  String line;
  serializeJson(env, line);
  line += '\n';
  client_.print(line);
  Serial.printf("[ctrl] HELLO sent (%u bytes)\n",
                static_cast<unsigned>(line.length()));
}

void ControlClient::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  ensureConnected();

  while (client_.connected() && client_.available() > 0) {
    String line = client_.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) {
      continue;
    }
    Serial.printf("[ctrl] RX %s\n", line.c_str());
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) {
      continue;
    }
    const char *msg = doc["msg"] | "";
    if (strcmp(msg, "assign_role") == 0) {
      const char *r = doc["body"]["role"] | "unassigned";
      role_ = roleFromName(r);
      Serial.printf("[ctrl] role assigned -> %s\n", roleName(role_));
      JsonDocument ack;
      ack["v"] = 1;
      ack["msg"] = "role_ack";
      ack["ts_us"] = static_cast<uint64_t>(esp_timer_get_time());
      ack["node_id"] = device_id_;
      ack["body"]["role"] = roleName(role_);
      String out;
      serializeJson(ack, out);
      out += '\n';
      client_.print(out);
    } else if (strcmp(msg, "ping") == 0) {
      JsonDocument pong;
      pong["v"] = 1;
      pong["msg"] = "pong";
      pong["ts_us"] = static_cast<uint64_t>(esp_timer_get_time());
      pong["node_id"] = device_id_;
      String out;
      serializeJson(pong, out);
      out += '\n';
      client_.print(out);
    }
  }

  const uint32_t now = millis();
  if (client_.connected() &&
      (last_hello_ms_ == 0 || now - last_hello_ms_ >= HELLO_INTERVAL_MS)) {
    last_hello_ms_ = now;
    sendHello();
  }
}

} // namespace usb_analyzer
