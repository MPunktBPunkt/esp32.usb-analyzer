#include "host/UsbHostNode.h"

#if ANALYZER_ENABLE_USB_HOST

#include <EspUsbHost.h>
#include <espusbhost_version.h>

#include <ArduinoJson.h>

namespace usb_analyzer {
namespace {

EspUsbHost g_usb;

const char *speedName(usb_speed_t speed) {
  switch (speed) {
  case USB_SPEED_LOW:
    return "low";
  case USB_SPEED_FULL:
    return "full";
  case USB_SPEED_HIGH:
    return "high";
  default:
    return "unknown";
  }
}

String deviceEventJson(const EspUsbHostDeviceInfo &d, const char *event) {
  JsonDocument doc;
  doc["event"] = event;
  doc["address"] = d.address;
  doc["vid"] = d.vid;
  doc["pid"] = d.pid;
  char id[12];
  snprintf(id, sizeof(id), "%04x:%04x", d.vid, d.pid);
  doc["id"] = id;
  doc["manufacturer"] = d.manufacturer ? d.manufacturer : "";
  doc["product"] = d.product ? d.product : "";
  doc["serial"] = d.serial ? d.serial : "";
  doc["speed"] = speedName(d.speed);
  doc["device_class"] = d.deviceClass;
  doc["device_subclass"] = d.deviceSubClass;
  doc["device_protocol"] = d.deviceProtocol;
  doc["configuration_value"] = d.configurationValue;
  doc["configuration_interfaces"] = d.configurationInterfaceCount;
  doc["configuration_total_length"] = d.configurationTotalLength;
  doc["max_power_ma"] = static_cast<unsigned>(d.configurationMaxPower) * 2U;
  doc["self_powered"] = (d.configurationAttributes & 0x40) != 0;
  doc["supported"] = d.supported;
  doc["is_hub"] = d.isHub;
  doc["port_id"] = d.portId;

  JsonArray ifaces = doc["interfaces"].to<JsonArray>();
  EspUsbHostInterfaceInfo interfaces[ESP_USB_HOST_MAX_INTERFACES];
  const size_t n =
      g_usb.getInterfaces(d.address, interfaces, ESP_USB_HOST_MAX_INTERFACES);
  for (size_t i = 0; i < n; i++) {
    JsonObject o = ifaces.add<JsonObject>();
    o["number"] = interfaces[i].number;
    o["class"] = interfaces[i].interfaceClass;
    o["subclass"] = interfaces[i].interfaceSubClass;
    o["protocol"] = interfaces[i].interfaceProtocol;
    o["claimed"] = interfaces[i].claimed;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

void logConnected(const EspUsbHostDeviceInfo &d) {
  Serial.println();
  Serial.println("=== USB ENUMERATED (Gate 1) ===");
  Serial.printf("address=%u speed=%s\n", d.address, speedName(d.speed));
  Serial.printf("vid:pid=%04x:%04x\n", d.vid, d.pid);
  Serial.printf("mfr=\"%s\" product=\"%s\" serial=\"%s\"\n",
                d.manufacturer ? d.manufacturer : "",
                d.product ? d.product : "", d.serial ? d.serial : "");
  Serial.printf("device class=0x%02x subclass=0x%02x protocol=0x%02x\n",
                d.deviceClass, d.deviceSubClass, d.deviceProtocol);
  Serial.printf("config value=%u interfaces=%u total_len=%u max_power=%umA %s\n",
                d.configurationValue, d.configurationInterfaceCount,
                d.configurationTotalLength,
                static_cast<unsigned>(d.configurationMaxPower) * 2U,
                (d.configurationAttributes & 0x40) ? "self-powered"
                                                   : "bus-powered");
  Serial.printf("library supported=%s hub=%s\n", d.supported ? "yes" : "no",
                d.isHub ? "yes" : "no");

  EspUsbHostInterfaceInfo interfaces[ESP_USB_HOST_MAX_INTERFACES];
  const size_t n =
      g_usb.getInterfaces(d.address, interfaces, ESP_USB_HOST_MAX_INTERFACES);
  for (size_t i = 0; i < n; i++) {
    Serial.printf(
        "  iface[%u] num=%u class=0x%02x subclass=0x%02x protocol=0x%02x "
        "claimed=%s\n",
        static_cast<unsigned>(i), interfaces[i].number,
        interfaces[i].interfaceClass, interfaces[i].interfaceSubClass,
        interfaces[i].interfaceProtocol,
        interfaces[i].claimed ? "yes" : "no");
  }
  Serial.println("CLAIM: host stack saw device descriptors (L1). Not wire "
                 "capture. Not MITM.");
  Serial.println("===============================");
}

} // namespace

bool UsbHostNode::begin() {
  Serial.println("[host] EspUsbHost bring-up");
  Serial.printf("[host] EspUsbHost %s\n", ESPUSBHOST_VERSION_STR);
  Serial.printf("[host] arduino-esp32 %s\n", ESP_ARDUINO_VERSION_STR);
  Serial.printf("[host] chip %s rev %u\n", ESP.getChipModel(),
                static_cast<unsigned>(ESP.getChipRevision()));
  Serial.println("[host] Debug path: UART/COM — OTG port is for USB DUT only");
  Serial.println("[host] VBUS: many DevKits do NOT source 5V — measure or use "
                 "powered hub");

  g_usb.onDeviceConnected([this](const EspUsbHostDeviceInfo &d) {
    logConnected(d);
    last_event_json_ = deviceEventJson(d, "enumerated");
    hint_printed_ = false;
  });

  g_usb.onDeviceDisconnected([this](const EspUsbHostDeviceInfo &d) {
    Serial.printf("[host] REMOVED address=%u %04x:%04x\n", d.address, d.vid,
                  d.pid);
    last_event_json_ = deviceEventJson(d, "removed");
  });

  begin_ok_ = g_usb.begin();
  if (begin_ok_) {
    Serial.println("[host] usb.begin(): ok — waiting for device");
  } else {
    Serial.printf("[host] usb.begin(): FAILED: %s\n", g_usb.lastErrorName());
    Serial.println("[host] This is a build/config problem, not cabling.");
  }

  ready_ = true;
  started_ms_ = millis();
  last_report_ms_ = started_ms_;
  return begin_ok_;
}

void UsbHostNode::loop() {
  if (!begin_ok_) {
    return;
  }
  const uint32_t now = millis();
  if (now - last_report_ms_ < 2000) {
    return;
  }
  last_report_ms_ = now;
  const size_t count = g_usb.deviceCount();
  Serial.printf("[host] t=%lus devices=%u channels=%u/%u heap=%u\n",
                static_cast<unsigned long>((now - started_ms_) / 1000UL),
                static_cast<unsigned>(count),
                static_cast<unsigned>(g_usb.endpointChannelCount()),
                static_cast<unsigned>(g_usb.maxEndpointChannelCount()),
                static_cast<unsigned>(ESP.getFreeHeap()));

  if (count == 0 && !hint_printed_ && (now - started_ms_) >= 10000) {
    hint_printed_ = true;
    printNoDeviceHints();
  }
}

size_t UsbHostNode::deviceCount() const {
  if (!begin_ok_) {
    return 0;
  }
  return g_usb.deviceCount();
}

void UsbHostNode::printNoDeviceHints() const {
  Serial.println();
  Serial.println("--- no device enumerated (check order) ---");
  Serial.println("1. VBUS 5V present on OTG connector?");
  Serial.println("2. Cable in USB-OTG port, not UART/COM?");
  Serial.println("3. Data cable (not charge-only)?");
  Serial.println("4. Try FS keyboard or flash drive first");
  Serial.println("5. HS-only devices will not enumerate on S3 Host (FACT)");
  Serial.println("------------------------------------------");
}

} // namespace usb_analyzer

#else // !ANALYZER_ENABLE_USB_HOST

namespace usb_analyzer {

bool UsbHostNode::begin() {
  Serial.println("[host] ANALYZER_ENABLE_USB_HOST=0 — host disabled");
  return false;
}
void UsbHostNode::loop() {}
size_t UsbHostNode::deviceCount() const { return 0; }
void UsbHostNode::printNoDeviceHints() const {}

} // namespace usb_analyzer

#endif
