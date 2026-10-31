#include "host/UsbHostNode.h"

#if ANALYZER_ENABLE_USB_HOST

#include <EspUsbHost.h>
#include <espusbhost_version.h>

#include "capture/CaptureLog.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <cstring>
#include <esp_timer.h>
#include <usb_analyzer/UsbLabPattern.h>

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

void UsbHostNode::clearDut() {
  dut_present_ = false;
  dut_vid_ = 0;
  dut_pid_ = 0;
  dut_address_ = 0;
  dut_mfr_ = "";
  dut_product_ = "";
  dut_serial_ = "";
  dut_speed_ = "";
}

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
    dut_present_ = true;
    dut_vid_ = d.vid;
    dut_pid_ = d.pid;
    dut_address_ = d.address;
    dut_mfr_ = d.manufacturer ? d.manufacturer : "";
    dut_product_ = d.product ? d.product : "";
    dut_serial_ = d.serial ? d.serial : "";
    dut_speed_ = speedName(d.speed);
    CaptureLog::instance().push(CapRole::Host, CapType::Attach, 0, 0, d.vid,
                                d.pid, 0, speedName(d.speed));
    CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, 0, 0, d.vid,
                                d.pid, 0, "descriptors");
  });

  g_usb.onDeviceDisconnected([this](const EspUsbHostDeviceInfo &d) {
    Serial.printf("[host] REMOVED address=%u %04x:%04x\n", d.address, d.vid,
                  d.pid);
    last_event_json_ = deviceEventJson(d, "removed");
    CaptureLog::instance().push(CapRole::Host, CapType::Detach, 0, 0, d.vid,
                                d.pid, 0, "removed");
    clearDut();
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

const char *UsbHostNode::otgState() const {
  if (!begin_ok_) {
    return "disabled";
  }
  if (deviceCount() > 0) {
    return "device_attached";
  }
  return "waiting_for_dut";
}

void UsbHostNode::appendStatusJson(String &json) const {
  json += ",\"dut\":{";
  json += "\"present\":";
  json += dut_present_ && deviceCount() > 0 ? "true" : "false";
  if (dut_present_ && deviceCount() > 0) {
    char id[12];
    snprintf(id, sizeof(id), "%04x:%04x", dut_vid_, dut_pid_);
    json += ",\"id\":\"";
    json += id;
    json += "\",\"vid\":";
    json += String(dut_vid_);
    json += ",\"pid\":";
    json += String(dut_pid_);
    json += ",\"address\":";
    json += String(dut_address_);
    json += ",\"speed\":\"";
    json += dut_speed_;
    json += "\",\"manufacturer\":\"";
    json += dut_mfr_;
    json += "\",\"product\":\"";
    json += dut_product_;
    json += "\",\"serial\":\"";
    json += dut_serial_;
    json += "\"";
  }
  json += "}";
  if (has_last_probe_) {
    const HostMscProbeResult &p = last_probe_;
    json += ",\"lastUsbProbe\":{";
    json += "\"ok\":";
    json += p.ok ? "true" : "false";
    json += ",\"msc_ready\":";
    json += p.msc_ready ? "true" : "false";
    json += ",\"pattern_ok\":";
    json += p.pattern_ok ? "true" : "false";
    json += ",\"blocks_read\":";
    json += String(p.blocks_read);
    json += ",\"bytes_read\":";
    json += String(p.bytes_read);
    json += ",\"duration_ms\":";
    json += String(p.duration_ms);
    json += ",\"rate_kBs\":";
    json += String(p.rate_kBs);
    json += ",\"path\":\"usb_otg_host_to_device_msc\"";
    if (p.error.length()) {
      json += ",\"err\":\"";
      json += p.error;
      json += "\"";
    }
    json += "}";
  }
}

HostMscProbeResult UsbHostNode::runMscProbe(uint32_t blocks, uint32_t start_lba,
                                            uint32_t chunk) {
  HostMscProbeResult r;
  if (!begin_ok_) {
    r.error = "host_disabled";
    last_probe_ = r;
    has_last_probe_ = true;
    return r;
  }
  if (deviceCount() == 0) {
    r.error = "no_dut";
    last_probe_ = r;
    has_last_probe_ = true;
    return r;
  }
  if (blocks == 0) {
    blocks = 64;
  }
  if (blocks > 2048) {
    blocks = 2048;
  }
  if (chunk == 0) {
    chunk = 1;
  }
  if (chunk > 8) {
    chunk = 8; // 8×512 = 4 KiB transfer buffer
  }
  r.chunk = chunk;

  char id[12];
  snprintf(id, sizeof(id), "%04x:%04x", dut_vid_, dut_pid_);
  r.dut_id = id;

  if (!g_usb.mscWaitReady(ESP_USB_HOST_ANY_ADDRESS, 3000, 1000)) {
    r.error = "msc_not_ready";
    last_probe_ = r;
    has_last_probe_ = true;
    return r;
  }
  r.msc_ready = true;

  EspUsbHostMscInquiry inq{};
  if (g_usb.mscInquiry(inq)) {
    r.inquiry_vendor = inq.vendor;
    r.inquiry_product = inq.product;
    r.inquiry_revision = inq.revision;
  }

  uint64_t cap_blocks = 0;
  uint32_t block_size = 0;
  if (!g_usb.mscCapacity64(cap_blocks, block_size)) {
    r.error = "capacity_fail";
    last_probe_ = r;
    has_last_probe_ = true;
    return r;
  }
  r.capacity_blocks = cap_blocks;
  r.block_size = block_size ? block_size : 512;

  if (start_lba + blocks > cap_blocks && cap_blocks > 0) {
    if (start_lba >= cap_blocks) {
      start_lba = 0;
    }
    blocks = static_cast<uint32_t>(
        std::min<uint64_t>(blocks, cap_blocks - start_lba));
  }

  static uint8_t buf[512 * 8];
  static uint32_t rtts[512];
  uint32_t rtt_n = 0;
  bool pattern_all = true;
  bool any_pattern_checked = false;
  uint32_t fail_bytes = 0;
  uint32_t done_blocks = 0;

  const uint32_t t0 = millis();
  while (done_blocks < blocks) {
    uint32_t n = chunk;
    if (done_blocks + n > blocks) {
      n = blocks - done_blocks;
    }
    const uint32_t lba = start_lba + done_blocks;
    memset(buf, 0, n * r.block_size);
    const int64_t t_push = esp_timer_get_time();
    const bool ok = g_usb.mscReadBlocks(lba, buf, n);
    const int64_t t_done = esp_timer_get_time();
    if (!ok) {
      r.error = "read_fail_lba_" + String(lba) + "_n_" + String(n);
      r.blocks_read = done_blocks;
      r.duration_ms = millis() - t0;
      last_probe_ = r;
      has_last_probe_ = true;
      return r;
    }
    r.transfers++;
    if (rtt_n < 512u) {
      rtts[rtt_n++] = static_cast<uint32_t>(t_done - t_push);
    }
    r.bytes_read += n * r.block_size;
    if (r.block_size == 512) {
      any_pattern_checked = true;
      for (uint32_t b = 0; b < n; b++) {
        if (!checkLabSector(buf + b * 512, 512, lba + b, 0)) {
          pattern_all = false;
          // count mismatched bytes roughly
          for (uint32_t i = 0; i < 512; i++) {
            if (buf[b * 512 + i] != labSectorByte(lba + b, i)) {
              fail_bytes++;
            }
          }
        }
      }
    }
    done_blocks += n;
  }
  r.blocks_read = done_blocks;
  r.duration_ms = millis() - t0;
  r.pattern_fail_bytes = fail_bytes;
  r.pattern_ok = any_pattern_checked && pattern_all;
  if (r.duration_ms > 0) {
    r.rate_kBs = static_cast<uint32_t>(
        (static_cast<uint64_t>(r.bytes_read) * 1000ull / r.duration_ms) /
        1024ull);
  }
  if (rtt_n > 0) {
    std::sort(rtts, rtts + rtt_n);
    uint64_t sum = 0;
    for (uint32_t i = 0; i < rtt_n; i++) {
      sum += rtts[i];
    }
    r.rtt_min_us = rtts[0];
    r.rtt_max_us = rtts[rtt_n - 1];
    r.rtt_avg_us = static_cast<uint32_t>(sum / rtt_n);
    r.rtt_p50_us = rtts[rtt_n / 2];
    r.rtt_p95_us = rtts[(rtt_n * 95) / 100];
  }
  r.ok = (r.blocks_read == blocks && r.error.length() == 0);
  last_probe_ = r;
  has_last_probe_ = true;
  CaptureLog::instance().push(
      CapRole::Host, CapType::MscRead, r.ok && r.pattern_ok ? 0 : 1, r.bytes_read,
      start_lba, blocks, r.duration_ms * 1000u,
      r.pattern_ok ? "probe_ok" : "probe_pat_fail");
  Serial.printf(
      "[host] MSC probe %s blocks=%u chunk=%u xfers=%u %u ms %u kB/s "
      "pattern=%s fail_B=%u\n",
      r.ok ? "OK" : "FAIL", static_cast<unsigned>(r.blocks_read),
      static_cast<unsigned>(r.chunk), static_cast<unsigned>(r.transfers),
      static_cast<unsigned>(r.duration_ms), static_cast<unsigned>(r.rate_kBs),
      r.pattern_ok ? "ok" : "mismatch",
      static_cast<unsigned>(r.pattern_fail_bytes));
  return r;
}

bool UsbHostNode::readBlocks(uint32_t lba, uint32_t blocks, uint8_t *out) {
  if (!begin_ok_ || !out || blocks == 0 || blocks > 8 || deviceCount() == 0) {
    return false;
  }
  if (!g_usb.mscWaitReady(ESP_USB_HOST_ANY_ADDRESS, 2000, 1000)) {
    return false;
  }
  const int64_t t0 = esp_timer_get_time();
  const bool ok = g_usb.mscReadBlocks(lba, out, blocks);
  const uint32_t dur = static_cast<uint32_t>(esp_timer_get_time() - t0);
  CaptureLog::instance().push(CapRole::Host, CapType::MscRead, ok ? 0 : 1,
                              blocks * 512u, lba, blocks, dur,
                              ok ? "proxy_read" : "proxy_fail");
  return ok;
}

void UsbHostNode::printNoDeviceHints() const {
  Serial.println();
  Serial.println("--- no device enumerated (check order) ---");
  Serial.println("1. VBUS 5V present on OTG connector?");
  Serial.println("2. Cable in USB-OTG port, not UART/COM?");
  Serial.println("3. Data cable (not charge-only)?");
  Serial.println("4. Try FS keyboard or flash drive first");
  Serial.println("5. HS-only devices will not enumerate on S3 Host (FACT)");
  Serial.println("6. OTG↔OTG loop: Device ESP must run gadget FW");
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
const char *UsbHostNode::otgState() const { return "disabled"; }
void UsbHostNode::appendStatusJson(String &json) const {
  json += ",\"dut\":{\"present\":false}";
  (void)json;
}
HostMscProbeResult UsbHostNode::runMscProbe(uint32_t, uint32_t, uint32_t) {
  HostMscProbeResult r;
  r.error = "host_disabled";
  last_probe_ = r;
  has_last_probe_ = true;
  return r;
}
bool UsbHostNode::readBlocks(uint32_t, uint32_t, uint8_t *) { return false; }
void UsbHostNode::printNoDeviceHints() const {}
void UsbHostNode::clearDut() {}

} // namespace usb_analyzer

#endif
