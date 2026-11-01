#include "host/UsbHostNode.h"

#if ANALYZER_ENABLE_USB_HOST

#include <EspUsbHost.h>
#include <espusbhost_version.h>

#include "capture/CaptureLog.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <cstring>
#include <esp_timer.h>
#include <esp_cpu.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/usb_dwc_ll.h>
#include <usb_analyzer/BclLabFrame.h>
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
  dut_cfg_ = 0;
  dut_iface_count_ = 0;
  dut_hid_iface_ = false;
}

void UsbHostNode::noteHidReport(uint8_t keycode, uint8_t ascii,
                                uint8_t modifiers, uint8_t iface,
                                const uint8_t *data, size_t len) {
  hid_keycode_ = keycode;
  hid_ascii_ = ascii;
  hid_modifiers_ = modifiers;
  hid_iface_ = iface;
  hid_report_len_ = static_cast<uint8_t>(len > sizeof(hid_report_)
                                             ? sizeof(hid_report_)
                                             : len);
  if (data && hid_report_len_) {
    memcpy(hid_report_, data, hid_report_len_);
  }
  hid_last_ms_ = millis();
  hid_report_count_++;
  hid_got_ = true;
  CaptureLog::instance().pushPayload(
      CapRole::Host, CapType::HidInput, 0, hid_report_len_, keycode, ascii, 0,
      "hid_irq", hid_report_, hid_report_len_);
}

bool UsbHostNode::fillDutSnapshot(DutInfoSnapshot &out) {
  memset(&out, 0, sizeof(out));
  if (!begin_ok_ || !dut_present_ || deviceCount() == 0) {
    return false;
  }
  out.present = 1;
  out.address = dut_address_;
  out.vid = dut_vid_;
  out.pid = dut_pid_;
  if (dut_speed_ == "low") {
    out.speed = 1;
  } else if (dut_speed_ == "full") {
    out.speed = 2;
  } else if (dut_speed_ == "high") {
    out.speed = 3;
  }
  strncpy(out.mfr, dut_mfr_.c_str(), sizeof(out.mfr) - 1);
  strncpy(out.product, dut_product_.c_str(), sizeof(out.product) - 1);
  strncpy(out.serial, dut_serial_.c_str(), sizeof(out.serial) - 1);

  // Use cached last MSC probe only — never block on USB I/O inside TCP
  // handlers (deadlocks Host serviceInbound ↔ Device proxy client).
  if (has_last_probe_ && last_probe_.msc_ready) {
    out.msc_ready = 1;
    strncpy(out.inq_vendor, last_probe_.inquiry_vendor.c_str(),
            sizeof(out.inq_vendor) - 1);
    strncpy(out.inq_product, last_probe_.inquiry_product.c_str(),
            sizeof(out.inq_product) - 1);
    strncpy(out.inq_rev, last_probe_.inquiry_revision.c_str(),
            sizeof(out.inq_rev) - 1);
    out.capacity_blocks = static_cast<uint32_t>(
        last_probe_.capacity_blocks > 0xffffffffull
            ? 0xffffffffull
            : last_probe_.capacity_blocks);
    out.block_size = static_cast<uint16_t>(
        last_probe_.block_size ? last_probe_.block_size : 512);
  }
  return true;
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
    dut_cfg_ = d.configurationValue;
    dut_iface_count_ = d.configurationInterfaceCount;
    dut_hid_iface_ = false;
    EspUsbHostInterfaceInfo interfaces[ESP_USB_HOST_MAX_INTERFACES];
    const size_t n =
        g_usb.getInterfaces(d.address, interfaces, ESP_USB_HOST_MAX_INTERFACES);
    if (n > 0) {
      dut_iface_count_ = static_cast<uint8_t>(n);
    }
    for (size_t i = 0; i < n; i++) {
      if (interfaces[i].interfaceClass == 0x03) {
        dut_hid_iface_ = true;
        break;
      }
    }
    CaptureLog::instance().push(CapRole::Host, CapType::Attach, 0, 0, d.vid,
                                d.pid, 0, speedName(d.speed));
    // note carries truncated product for Gate-7 inspector (≤23 chars)
    char prod[24] = {0};
    if (d.product) {
      strncpy(prod, d.product, sizeof(prod) - 1);
    } else {
      snprintf(prod, sizeof(prod), "%04x:%04x", d.vid, d.pid);
    }
    CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, 0, d.address,
                                d.vid, d.pid, 0, prod);
    // Post-enum outcome: address + bConfigurationValue (NOT mid-enum SETUP).
    CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, 0, dut_cfg_,
                                d.address, dut_iface_count_, 0,
                                dut_hid_iface_ ? "cfg_hid" : "cfg_outcome");
  });

  g_usb.onDeviceDisconnected([this](const EspUsbHostDeviceInfo &d) {
    Serial.printf("[host] REMOVED address=%u %04x:%04x\n", d.address, d.vid,
                  d.pid);
    last_event_json_ = deviceEventJson(d, "removed");
    CaptureLog::instance().push(CapRole::Host, CapType::Detach, 0, 0, d.vid,
                                d.pid, 0, "removed");
    clearDut();
  });

  g_usb.onKeyboard([this](const EspUsbHostKeyboardEvent &ev) {
    if (ev.pressed || ev.keycode != 0 || ev.ascii != 0) {
      noteHidReport(ev.keycode, ev.ascii, ev.modifiers, ev.interfaceNumber,
                    ev.rawData ? ev.rawData : ev.reportData,
                    ev.rawData ? ev.rawLength : ev.reportLength);
    }
  });
  g_usb.onHIDInput([this](const EspUsbHostHIDInput &in) {
    // Fallback path if keyboard parser does not fire (raw interrupt report).
    if (in.length == 0 || !in.data) {
      return;
    }
    bool nonzero = false;
    for (size_t i = 0; i < in.length; i++) {
      if (in.data[i]) {
        nonzero = true;
        break;
      }
    }
    if (!nonzero) {
      return; // ignore empty / key-up
    }
    uint8_t kc = 0;
    if (in.length >= 3) {
      kc = in.data[2]; // boot keyboard key[0]
    }
    noteHidReport(kc, 0, in.length >= 1 ? in.data[0] : 0, in.interfaceNumber,
                  in.data, in.length);
  });

  begin_ok_ = g_usb.begin();
  host_released_ = false;
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

bool UsbHostNode::releaseHostStack(String &err) {
  if (host_released_) {
    return true;
  }
  if (!begin_ok_) {
    host_released_ = true;
    begin_ok_ = false;
    return true;
  }
  Serial.println("[host] releasing USB Host stack for wire stimulus");
  g_usb.end();
  begin_ok_ = false;
  host_released_ = true;
  clearDut();
  delay(150);
  Serial.printf("[host] after end(): ready=%d — GPIO19/20 may be claimed\n",
                g_usb.ready() ? 1 : 0);
  (void)err;
  return true;
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
    json += ",\"configurationValue\":";
    json += String(dut_cfg_);
    json += ",\"interfaces\":";
    json += String(dut_iface_count_);
    json += ",\"hidIface\":";
    json += dut_hid_iface_ ? "true" : "false";
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
  json += ",\"hid\":{";
  json += "\"reports\":";
  json += String(hid_report_count_);
  json += ",\"lastKeycode\":";
  json += String(hid_keycode_);
  json += ",\"lastAscii\":";
  json += String(hid_ascii_);
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
  if (!begin_ok_ || !out || blocks == 0 || blocks > 16 || deviceCount() == 0) {
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
  CaptureLog::instance().push(CapRole::Host, CapType::BotCbw, ok ? 0 : 1, 31, lba,
                              blocks, 0, "READ10_inferred");
  CaptureLog::instance().push(CapRole::Host, CapType::BotCsw, ok ? 0 : 1, 13, lba,
                              0, dur, ok ? "CSW_ok_inferred" : "CSW_fail");
  return ok;
}

UsbHostNode::FaultObserveResult
UsbHostNode::runFaultObserve(uint32_t lba, uint32_t blocks, bool do_sense,
                             bool do_recovery) {
  FaultObserveResult r;
  const uint32_t t0 = millis();
  if (!begin_ok_ || host_released_ || deviceCount() == 0) {
    r.error = "no_dut";
    r.duration_ms = millis() - t0;
    return r;
  }
  if (blocks == 0 || blocks > 16) {
    r.error = "blocks_1_16";
    r.duration_ms = millis() - t0;
    return r;
  }
  uint8_t *buf = static_cast<uint8_t *>(malloc(blocks * 512u));
  if (!buf) {
    r.error = "oom";
    r.duration_ms = millis() - t0;
    return r;
  }
  const uint32_t t_read = millis();
  r.read_ok = readBlocks(lba, blocks, buf);
  r.read_ms = millis() - t_read;

  auto fill_sense = [&](bool after_recovery) {
    EspUsbHostMscSense sense{};
    bool got = g_usb.mscLastSense(sense);
    if (!got || (sense.senseKey == 0 && sense.additionalSenseCode == 0)) {
      got = g_usb.mscRequestSense(sense);
    }
    if (!got) {
      return;
    }
    r.sense_ok = true;
    r.sense_key = sense.senseKey;
    r.asc = sense.additionalSenseCode;
    r.asq = sense.additionalSenseQualifier;
    r.response_code = sense.responseCode;
    if (after_recovery) {
      r.sense_after_ok = true;
      r.sense_key_after = sense.senseKey;
      r.asc_after = sense.additionalSenseCode;
      if (r.sense_phase.length() == 0) {
        r.sense_phase = "after_recovery";
      }
    } else {
      r.sense_before_ok = true;
      r.sense_key_before = sense.senseKey;
      r.asc_before = sense.additionalSenseCode;
      r.sense_phase = "before_recovery";
    }
    CaptureLog::instance().push(
        CapRole::Host, CapType::Error, sense.senseKey, sense.additionalSenseCode,
        sense.additionalSenseQualifier, sense.responseCode, r.read_ms,
        after_recovery ? "msc_sense_after" : "msc_sense_before");
  };

  // Sense first — EspUsbHost may already have cached it after failed CSW.
  if (!r.read_ok && do_sense) {
    fill_sense(false);
  }

  if (do_recovery) {
    const uint32_t t_rec = millis();
    // EspUsbHost may BOT-reset stalled pipes; retry once after wait.
    (void)g_usb.mscWaitReady(ESP_USB_HOST_ANY_ADDRESS, 1500, 1000);
    r.recovery_ok = readBlocks(lba, blocks, buf);
    if (!r.recovery_ok) {
      delay(120);
      (void)g_usb.mscWaitReady(ESP_USB_HOST_ANY_ADDRESS, 1500, 1000);
      r.recovery_ok = readBlocks(lba, blocks, buf);
    }
    r.recovery_ms = millis() - t_rec;
  }

  // Sense after recovery — often empty (BOT-reset cleared condition).
  if (!r.read_ok && do_sense) {
    fill_sense(true);
  }
  if (r.sense_phase.length() == 0) {
    r.sense_phase = "none";
  }
  free(buf);
  r.ok = true;
  r.duration_ms = millis() - t0;
  return r;
}

bool UsbHostNode::writeBlocks(uint32_t lba, uint32_t blocks, const uint8_t *in) {
  if (!begin_ok_ || !in || blocks == 0 || blocks > 16 || deviceCount() == 0) {
    return false;
  }
  if (!g_usb.mscWaitReady(ESP_USB_HOST_ANY_ADDRESS, 2000, 1000)) {
    return false;
  }
  const int64_t t0 = esp_timer_get_time();
  const bool ok = g_usb.mscWriteBlocks(lba, in, blocks);
  const uint32_t dur = static_cast<uint32_t>(esp_timer_get_time() - t0);
  CaptureLog::instance().push(CapRole::Host, CapType::MscWrite, ok ? 0 : 1,
                              blocks * 512u, lba, blocks, dur,
                              ok ? "host_write" : "write_fail");
  CaptureLog::instance().push(CapRole::Host, CapType::BotCbw, ok ? 0 : 1, 31, lba,
                              blocks, 0, "WRITE10_inferred");
  CaptureLog::instance().push(CapRole::Host, CapType::BotCsw, ok ? 0 : 1, 13, lba,
                              0, dur, ok ? "CSW_ok_inferred" : "CSW_fail");
  return ok;
}

HostMscProbeResult UsbHostNode::runMscWriteProbe(uint32_t blocks,
                                                 uint32_t start_lba,
                                                 uint32_t chunk,
                                                 bool readback) {
  HostMscProbeResult r;
  if (!begin_ok_ || deviceCount() == 0) {
    r.error = begin_ok_ ? "no_dut" : "host_disabled";
    last_probe_ = r;
    has_last_probe_ = true;
    return r;
  }
  if (blocks == 0) {
    blocks = 64;
  }
  if (blocks > 256) {
    blocks = 256; // device write-cache is 64 LBAs — keep probe modest
  }
  if (chunk == 0) {
    chunk = 1;
  }
  if (chunk > 8) {
    chunk = 8;
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
  r.block_size = 512;

  static uint8_t buf[512 * 8];
  static uint8_t rb[512 * 8];
  uint32_t done = 0;
  bool pattern_all = true;
  const uint32_t t0 = millis();
  while (done < blocks) {
    uint32_t n = chunk;
    if (done + n > blocks) {
      n = blocks - done;
    }
    const uint32_t lba = start_lba + done;
    for (uint32_t b = 0; b < n; b++) {
      fillLabSector(buf + b * 512, 512, lba + b, 0);
      // Distinct write marker so readback ≠ default pattern alone
      buf[b * 512] = static_cast<uint8_t>(0xA5 ^ (lba + b));
    }
    if (!writeBlocks(lba, n, buf)) {
      r.error = "write_fail_lba_" + String(lba);
      r.blocks_read = done;
      r.duration_ms = millis() - t0;
      last_probe_ = r;
      has_last_probe_ = true;
      return r;
    }
    r.transfers++;
    r.bytes_read += n * 512u; // misuse field as bytes_xfer for write probe
    if (readback) {
      memset(rb, 0, n * 512u);
      if (!readBlocks(lba, n, rb) || memcmp(buf, rb, n * 512u) != 0) {
        pattern_all = false;
        r.pattern_fail_bytes += n * 512u;
      }
    }
    done += n;
  }
  r.blocks_read = done;
  r.duration_ms = millis() - t0;
  r.pattern_ok = readback ? pattern_all : true;
  if (r.duration_ms > 0) {
    r.rate_kBs = static_cast<uint32_t>(
        (static_cast<uint64_t>(r.bytes_read) * 1000ull / r.duration_ms) /
        1024ull);
  }
  r.ok = (r.blocks_read == blocks && r.error.length() == 0);
  last_probe_ = r;
  has_last_probe_ = true;
  CaptureLog::instance().push(
      CapRole::Host, CapType::MscWrite, r.ok && r.pattern_ok ? 0 : 1, r.bytes_read,
      start_lba, blocks, r.duration_ms * 1000u,
      r.pattern_ok ? "write_probe_ok" : "write_probe_fail");
  return r;
}

bool UsbHostNode::controlTransfer(uint8_t bm_request_type, uint8_t b_request,
                                  uint16_t w_value, uint16_t w_index,
                                  uint8_t *data, uint16_t w_length,
                                  uint16_t *actual_len) {
  if (!begin_ok_ || deviceCount() == 0) {
    return false;
  }
  if (w_length > 64) {
    return false;
  }
  size_t actual = 0;
  const int64_t t0 = esp_timer_get_time();
  const bool ok = g_usb.vendorControlTransfer(
      bm_request_type, b_request, w_value, w_index, data, w_length, &actual,
      ESP_USB_HOST_ANY_ADDRESS);
  const uint32_t dur = static_cast<uint32_t>(esp_timer_get_time() - t0);
  if (actual_len) {
    *actual_len = static_cast<uint16_t>(actual);
  }
  CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, ok ? 0 : 1,
                              static_cast<uint32_t>(actual), w_value, w_index,
                              dur, ok ? "ep0_ctrl" : "ep0_fail");
  return ok;
}

UsbHostNode::AoaHandshakeResult
UsbHostNode::runAoaHandshake(const char *profile, bool wait_reenum) {
  AoaHandshakeResult r;
  r.profile = profile ? profile : "lab";
  const uint32_t t0 = millis();

  // String profiles — Desk/lab only (hu-probe lab_aoa_iface STRING_PROFILES).
  // bmw_lab is hypothesis for Connected experiments — NOT measured NBT values.
  const char *strings[6] = {"LabHost", "usb-analyzer", "Lab AOA Host", "0.1",
                            "https://local/lab", "LAB001"};
  if (r.profile == "bmw_lab") {
    strings[0] = "BMW";
    strings[1] = "NBT Evo";
    strings[2] = "Headunit";
    strings[3] = "1.0";
    strings[4] = "http://www.bmw.com";
    strings[5] = "USB-ANALYZER-LAB";
  } else if (r.profile == "minimal") {
    strings[0] = "Android";
    strings[1] = "AOA";
    strings[2] = "Accessory";
    strings[3] = "1.0";
    strings[4] = "http://localhost";
    strings[5] = "1";
  }

  if (!begin_ok_ || deviceCount() == 0) {
    r.error = "no_dut";
    r.duration_ms = millis() - t0;
    return r;
  }
  r.dut_vid_before = dut_vid_;
  r.dut_pid_before = dut_pid_;

  // 51 GET_PROTOCOL — IN | VENDOR
  uint8_t proto[2] = {0, 0};
  uint16_t actual = 0;
  if (!controlTransfer(0xC0, 51, 0, 0, proto, 2, &actual) || actual < 2) {
    r.error = "aoa51_fail";
    r.duration_ms = millis() - t0;
    CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, 1, actual, 51,
                                0, 0, "aoa51_fail");
    return r;
  }
  r.protocol = static_cast<uint16_t>(proto[0] | (proto[1] << 8));
  r.protocol_ok = (r.protocol != 0);
  CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, 0, actual, 51,
                              r.protocol, 0, "aoa51_ok");
  if (!r.protocol_ok) {
    r.error = "protocol_zero";
    r.duration_ms = millis() - t0;
    return r;
  }

  // 52 SEND_STRING × 6 — OUT | VENDOR, index = string ID
  for (uint8_t i = 0; i < 6; i++) {
    uint8_t buf[64];
    size_t n = strlen(strings[i]);
    if (n > 62) {
      n = 62;
    }
    memcpy(buf, strings[i], n);
    buf[n] = 0; // AOSP: zero-terminated
    uint16_t act = 0;
    if (!controlTransfer(0x40, 52, 0, i, buf, static_cast<uint16_t>(n + 1),
                         &act)) {
      r.error = String("aoa52_fail_idx_") + String(i);
      r.duration_ms = millis() - t0;
      return r;
    }
    r.strings_ok++;
  }

  // 53 START — OUT | VENDOR, no data
  actual = 0;
  if (!controlTransfer(0x40, 53, 0, 0, nullptr, 0, &actual)) {
    r.error = "aoa53_fail";
    r.duration_ms = millis() - t0;
    return r;
  }
  r.start_ok = true;
  CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, 0, 0, 53, 0, 0,
                              "aoa53_ok");

  if (wait_reenum) {
    // Soft-reenum on Device ~80ms + host rediscovery
    for (int i = 0; i < 40; i++) {
      delay(50);
      loop(); // refresh DUT cache
      if (dut_present_ && dut_vid_ == 0x18D1 &&
          (dut_pid_ == 0x2D00 || dut_pid_ == 0x2D01)) {
        r.reenum_ok = true;
        break;
      }
    }
    r.dut_vid_after = dut_vid_;
    r.dut_pid_after = dut_pid_;
    if (!r.reenum_ok) {
      r.error = "reenum_timeout";
    }
  } else {
    r.reenum_ok = true; // not required
  }

  r.ok = r.protocol_ok && r.strings_ok == 6 && r.start_ok &&
         (!wait_reenum || r.reenum_ok);
  r.duration_ms = millis() - t0;
  return r;
}

UsbHostNode::AoaBulkResult UsbHostNode::runAoaBulkEcho(const char *payload) {
  const char *msg = (payload && payload[0]) ? payload : "AOA-LAB";
  return runAoaBulkEcho(reinterpret_cast<const uint8_t *>(msg), strlen(msg));
}

UsbHostNode::AoaBulkResult UsbHostNode::runAoaBulkEcho(const uint8_t *data,
                                                       size_t n) {
  AoaBulkResult r;
  const uint32_t t0 = millis();
  if (!begin_ok_ || deviceCount() == 0 || !dut_present_) {
    r.error = "no_dut";
    r.duration_ms = millis() - t0;
    return r;
  }
  if (!data || n == 0 || n > 64) {
    r.error = "bad_payload";
    r.duration_ms = millis() - t0;
    return r;
  }

  if (!g_usb.vendorOpen(ESP_USB_HOST_ANY_ADDRESS, 0xff,
                        ESP_USB_HOST_VENDOR_READ_CONTINUOUS, 64)) {
    r.error = "vendor_open_fail";
    r.duration_ms = millis() - t0;
    CaptureLog::instance().push(CapRole::Host, CapType::Error, 1, 0, dut_vid_,
                                dut_pid_, 0, "aoa_bulk_open");
    return r;
  }
  r.opened = true;
  r.ep_out = g_usb.vendorOutEndpoint();
  r.ep_in = g_usb.vendorInEndpoint();
  r.pkt_out = g_usb.vendorOutPacketSize();
  r.pkt_in = g_usb.vendorInPacketSize();
  if (r.ep_out == 0 || r.ep_in == 0) {
    r.error = "vendor_ep_missing";
    r.duration_ms = millis() - t0;
    return r;
  }

  delay(20);
  if (!g_usb.vendorWrite(data, n)) {
    r.error = "vendor_write_fail";
    r.duration_ms = millis() - t0;
    return r;
  }
  r.tx = static_cast<uint32_t>(n);
  CaptureLog::instance().push(CapRole::Host, CapType::TcpBulk, 0, r.tx, 0, 0, 0,
                              "aoa_bulk_out");

  uint8_t buf[64];
  memset(buf, 0, sizeof(buf));
  size_t actual = 0;
  const uint32_t deadline = millis() + 2000;
  while (millis() < deadline && actual < n) {
    size_t got = g_usb.vendorRead(buf + actual, sizeof(buf) - actual);
    if (got > 0) {
      actual += got;
      continue;
    }
    delay(10);
  }
  if (actual == 0) {
    size_t sync_got = 0;
    if (g_usb.vendorReadSync(buf, sizeof(buf), &sync_got, 500)) {
      actual = sync_got;
    }
  }
  if (actual == 0) {
    r.error = "vendor_read_fail";
    r.duration_ms = millis() - t0;
    return r;
  }
  r.rx = static_cast<uint32_t>(actual);
  r.rx_len = static_cast<uint8_t>(actual > 64 ? 64 : actual);
  memcpy(r.rx_buf, buf, r.rx_len);
  r.match = (actual == n && memcmp(buf, data, n) == 0);
  CaptureLog::instance().push(CapRole::Host, CapType::TcpBulk, r.match ? 0 : 1,
                              r.rx, r.tx, 0, 0,
                              r.match ? "aoa_bulk_in_ok" : "aoa_bulk_mismatch");
  r.ok = r.match;
  if (!r.ok && r.error.length() == 0) {
    r.error = "echo_mismatch";
  }
  r.duration_ms = millis() - t0;
  return r;
}

UsbHostNode::AoaBclResult UsbHostNode::runAoaBclRoundtrip(
    uint16_t cmd, uint16_t dest, bool send_init, const uint8_t *payload,
    size_t payload_len, bool explicit_payload) {
  AoaBclResult r;
  const uint32_t t0 = millis();

  if (send_init) {
    AoaBulkResult init = runAoaBulkEcho(kBclLabInit, sizeof(kBclLabInit));
    r.init_ok = init.ok;
    if (!init.ok) {
      r.error = String("init_") + (init.error.length() ? init.error : "fail");
      r.duration_ms = millis() - t0;
      return r;
    }
    CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, 0, 4, 0x1234,
                                0x5678, 0, "bcl_init");
  } else {
    r.init_ok = true;
  }

  // Default lab payload unless caller passes explicit (may be empty).
  const uint8_t default_pl[6] = {0x00, 0x01, 0x1A, 0xB1, 0x10, 0x00};
  const uint8_t *pl = payload;
  size_t pl_len = payload_len;
  if (!explicit_payload) {
    pl = default_pl;
    pl_len = sizeof(default_pl);
  }
  if (pl_len > 56) {
    r.error = "payload_too_long";
    r.duration_ms = millis() - t0;
    return r;
  }

  BclLabHeader hdr{};
  hdr.command = cmd;
  hdr.src = 0x0001;
  hdr.dest = dest;
  hdr.data_len = static_cast<uint16_t>(pl_len);

  uint8_t frame[64];
  const size_t flen = bclPack(frame, sizeof(frame), hdr, pl, pl_len);
  if (flen == 0) {
    r.error = "pack_fail";
    r.duration_ms = millis() - t0;
    return r;
  }

  AoaBulkResult round = runAoaBulkEcho(frame, flen);
  r.tx = round.tx;
  r.rx = round.rx;
  r.frame_ok = round.ok;
  if (!round.ok) {
    r.error = String("frame_") + (round.error.length() ? round.error : "fail");
    r.duration_ms = millis() - t0;
    return r;
  }

  // Parse **RX** buffer (independent of TX frame pointer).
  BclLabHeader got{};
  r.parse_ok = bclValidateFrame(round.rx_buf, round.rx_len, got) &&
               (got.command == cmd) && (got.dest == dest) &&
               (got.src == hdr.src) && (got.data_len == hdr.data_len);
  r.cmd = got.command;
  r.src = got.src;
  r.dest = got.dest;
  r.data_len = got.data_len;
  CaptureLog::instance().pushPayload(
      CapRole::Host, CapType::EnumInfo, r.parse_ok ? 0 : 1, round.rx_len,
      got.command, got.dest, 0, "bcl_frame", round.rx_buf,
      round.rx_len >= 8 ? 8 : round.rx_len);
  r.ok = r.init_ok && r.frame_ok && r.parse_ok;
  if (!r.ok && r.error.length() == 0) {
    r.error = "parse_fail";
  }
  r.duration_ms = millis() - t0;
  return r;
}

UsbHostNode::EnumObserveResult UsbHostNode::runEnumObserve(bool ep0_probe) {
  EnumObserveResult r;
  const uint32_t t0 = millis();
  r.dut_present = dut_present_ && deviceCount() > 0;
  if (!r.dut_present) {
    r.error = begin_ok_ ? "no_dut" : "host_disabled";
    r.duration_ms = millis() - t0;
    return r;
  }
  r.address = dut_address_;
  r.configuration_value = dut_cfg_;
  r.iface_count = dut_iface_count_;
  r.hid_iface = dut_hid_iface_;

  if (ep0_probe) {
    uint8_t desc[18];
    memset(desc, 0, sizeof(desc));
    uint16_t actual = 0;
    // GET_DESCRIPTOR(DEVICE) — caller-initiated EP0, not mid-enum sniff.
    r.get_desc_ok =
        controlTransfer(0x80, 0x06, 0x0100, 0, desc, 18, &actual) && actual >= 18;

    uint8_t cfg = 0;
    actual = 0;
    // GET_CONFIGURATION
    r.get_config_ok =
        controlTransfer(0x80, 0x08, 0, 0, &cfg, 1, &actual) && actual >= 1;
    r.get_config_byte = cfg;
    r.get_config_match =
        r.get_config_ok && (cfg == r.configuration_value) && (cfg != 0);
    CaptureLog::instance().push(
        CapRole::Host, CapType::EnumInfo, r.get_config_match ? 0 : 1, cfg,
        r.address, r.configuration_value, 0, "get_config");
  } else {
    r.get_desc_ok = true;
    r.get_config_ok = true;
    r.get_config_match = r.configuration_value != 0;
  }

  r.ok = r.dut_present && r.address != 0 && r.configuration_value != 0 &&
         (!ep0_probe || (r.get_desc_ok && r.get_config_match));
  if (!r.ok && r.error.length() == 0) {
    r.error = !r.get_desc_ok
                  ? "get_desc_fail"
                  : (!r.get_config_ok ? "get_config_fail" : "cfg_mismatch");
  }
  r.duration_ms = millis() - t0;
  return r;
}

UsbHostNode::SuspendResumeResult UsbHostNode::runBusSuspend(uint32_t hold_ms) {
  SuspendResumeResult r;
  r.hold_ms = hold_ms;
  const uint32_t t0 = millis();
  if (!begin_ok_ || deviceCount() == 0) {
    r.error = begin_ok_ ? "no_dut" : "host_disabled";
    r.duration_ms = millis() - t0;
    return r;
  }
  // Lab: poke DWC HPRT.prtsusp — EspUsbHost has no public suspend API.
  usb_dwc_ll_hprt_set_port_suspend(&USB_DWC);
  delay(5);
  r.suspended = usb_dwc_ll_hprt_get_port_suspend(&USB_DWC);
  CaptureLog::instance().push(CapRole::Host, CapType::Suspend,
                              r.suspended ? 0 : 1, hold_ms, 0, 0, 0,
                              "hprt_suspend");
  r.ok = r.suspended;
  if (!r.ok) {
    r.error = "hprt_suspend_clear";
  }
  r.duration_ms = millis() - t0;
  return r;
}

UsbHostNode::SuspendResumeResult UsbHostNode::runBusResume() {
  SuspendResumeResult r;
  const uint32_t t0 = millis();
  if (!begin_ok_) {
    r.error = "host_disabled";
    r.duration_ms = millis() - t0;
    return r;
  }
  // Drive resume ≥20 ms then clear (USB FS host requirement).
  usb_dwc_ll_hprt_set_port_resume(&USB_DWC);
  delay(25);
  usb_dwc_ll_hprt_clr_port_resume(&USB_DWC);
  delay(10);
  r.resumed = !usb_dwc_ll_hprt_get_port_suspend(&USB_DWC);
  CaptureLog::instance().push(CapRole::Host, CapType::Resume, r.resumed ? 0 : 1,
                              0, 0, 0, 0, "hprt_resume");
  r.ok = r.resumed;
  if (!r.ok) {
    r.error = "hprt_still_suspended";
  }
  r.duration_ms = millis() - t0;
  return r;
}

UsbHostNode::SuspendResumeResult
UsbHostNode::runBusSuspendResume(uint32_t hold_ms) {
  SuspendResumeResult r;
  r.hold_ms = hold_ms;
  const uint32_t t0 = millis();
  SuspendResumeResult s = runBusSuspend(hold_ms);
  r.suspended = s.suspended;
  if (!s.ok) {
    r.error = s.error;
    r.duration_ms = millis() - t0;
    return r;
  }
  // USB FS: device detects suspend after ~3 ms of bus idle; hold a bit longer.
  delay(hold_ms < 50 ? 50 : hold_ms);
  SuspendResumeResult u = runBusResume();
  r.resumed = u.resumed;
  if (!u.ok) {
    r.error = u.error;
    r.duration_ms = millis() - t0;
    return r;
  }
  // Post-resume EP0 smoke — retry; HPRT poke can briefly upset EspUsbHost pipes.
  uint8_t desc[18];
  uint16_t actual = 0;
  for (int attempt = 0; attempt < 5; attempt++) {
    memset(desc, 0, sizeof(desc));
    actual = 0;
    if (controlTransfer(0x80, 0x06, 0x0100, 0, desc, 18, &actual) &&
        actual >= 18) {
      r.post_traffic_ok = true;
      break;
    }
    delay(50);
  }
  r.ok = r.suspended && r.resumed && r.post_traffic_ok;
  if (!r.ok && r.error.length() == 0) {
    r.error = "post_resume_ep0_fail";
  }
  CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo,
                              r.ok ? 0 : 1, actual, r.hold_ms, 0, 0,
                              r.ok ? "susp_resume_ok" : "susp_resume_fail");
  r.duration_ms = millis() - t0;
  return r;
}

UsbHostNode::L0FeasibilityResult UsbHostNode::runL0Feasibility() {
  L0FeasibilityResult r;
  r.host_begun = begin_ok_;
  r.dut_present = begin_ok_ && deviceCount() > 0;
  // FACT: ESP32-S3 internal USB PHY uses GPIO20=D+, GPIO19=D−.
  // Path A (GPIO sniffer while OTG owns pins) cannot deliver FS packet capture.
  r.path_a_sniffer_ok = false;
  r.path_a_reason =
      "gpio19_20_are_internal_usb_phy_pins_cpu_sample_not_fs_packet_sniffer";
  r.note = "level2_hprt_only_not_l0_packets_prefer_path_b_external_tap";

  if (!begin_ok_) {
    r.error = "host_disabled";
    r.ok = true; // feasibility still reports Path A falsify
    return r;
  }

  r.hprt_raw = USB_DWC.hprt_reg.val;
  r.prt_conn = usb_dwc_ll_hprt_get_conn_status(&USB_DWC);
  r.prt_ena = usb_dwc_ll_hprt_get_port_en(&USB_DWC);
  r.prt_susp = usb_dwc_ll_hprt_get_port_suspend(&USB_DWC);
  r.prt_overcur = usb_dwc_ll_hprt_get_port_overcur(&USB_DWC);
  // prtspd: 0=HS(n/a), 1=FS, 2=LS on DWC — expose raw field
  r.prt_speed = static_cast<uint8_t>(USB_DWC.hprt_reg.prtspd);

  // Non-destructive level peek only — do not remux pins away from USB.
  r.gpio19_level = gpio_get_level(GPIO_NUM_19);
  r.gpio20_level = gpio_get_level(GPIO_NUM_20);
  r.ok = true;
  return r;
}

namespace {

struct E11SamplerShared {
  volatile bool run = true;
  volatile bool done = false;
  uint32_t target = 0;
  uint32_t samples = 0;
  uint32_t transitions_dm = 0;
  uint32_t transitions_dp = 0;
  uint32_t dt_min = 0;
  uint32_t dt_max = 0;
  uint64_t dt_sum = 0;
  uint32_t duration_us = 0;
};

void e11SampleTask(void *arg) {
  auto *s = static_cast<E11SamplerShared *>(arg);
  // Peek only — never gpio_set_direction / matrix remux (would kill OTG).
  int prev_dm = gpio_get_level(GPIO_NUM_19);
  int prev_dp = gpio_get_level(GPIO_NUM_20);
  uint32_t last_cy = esp_cpu_get_cycle_count();
  const int64_t t0 = esp_timer_get_time();
  s->dt_min = UINT32_MAX;
  s->dt_max = 0;
  s->dt_sum = 0;
  s->transitions_dm = 0;
  s->transitions_dp = 0;
  s->samples = 0;
  while (s->run && s->samples < s->target) {
    const int dm = gpio_get_level(GPIO_NUM_19);
    const int dp = gpio_get_level(GPIO_NUM_20);
    const uint32_t cy = esp_cpu_get_cycle_count();
    const uint32_t dcy = cy - last_cy;
    last_cy = cy;
    if (s->samples > 0) {
      if (dcy < s->dt_min) {
        s->dt_min = dcy;
      }
      if (dcy > s->dt_max) {
        s->dt_max = dcy;
      }
      s->dt_sum += dcy;
    }
    if (dm != prev_dm) {
      s->transitions_dm++;
      prev_dm = dm;
    }
    if (dp != prev_dp) {
      s->transitions_dp++;
      prev_dp = dp;
    }
    s->samples++;
  }
  s->duration_us =
      static_cast<uint32_t>(esp_timer_get_time() - t0);
  s->done = true;
  vTaskDelete(nullptr);
}

} // namespace

UsbHostNode::E11GpioSampleResult
UsbHostNode::runE11GpioSample(uint32_t samples, uint32_t traffic_blocks) {
  E11GpioSampleResult r;
  const uint32_t t0 = millis();
  r.host_begun = begin_ok_;
  r.cpu_mhz = getCpuFrequencyMhz();
  r.note = "peek_only_no_pin_remux;concurrent_msc_on_other_core";
  r.path_a_usable = false;
  r.path_a_reason =
      "cpu_gpio_poll_rate_and_phy_ownership_insufficient_for_fs_l0";

  if (!begin_ok_) {
    r.error = "host_disabled";
    r.ok = true;
    r.duration_ms = millis() - t0;
    return r;
  }

  if (samples < 1000) {
    samples = 1000;
  }
  if (samples > 2000000) {
    samples = 2000000;
  }
  if (traffic_blocks > 512) {
    traffic_blocks = 512;
  }

  r.dut_before = deviceCount() > 0;
  r.hprt_before = USB_DWC.hprt_reg.val;
  r.prt_conn_before = usb_dwc_ll_hprt_get_conn_status(&USB_DWC);
  r.prt_ena_before = usb_dwc_ll_hprt_get_port_en(&USB_DWC);

  E11SamplerShared shared;
  shared.target = samples;
  // Pin sampler on APP CPU (1); MSC/WiFi stay on PRO CPU (0).
  const BaseType_t created =
      xTaskCreatePinnedToCore(e11SampleTask, "e11gpio", 4096, &shared,
                              configMAX_PRIORITIES - 1, nullptr, 1);
  if (created != pdPASS) {
    r.error = "sampler_task_create_failed";
    r.duration_ms = millis() - t0;
    return r;
  }

  // Concurrent bus traffic while sampling (if DUT present).
  if (r.dut_before && traffic_blocks > 0) {
    const HostMscProbeResult msc = runMscProbe(traffic_blocks, 0, 4);
    r.traffic_ok = msc.ok && msc.pattern_ok;
    r.traffic_rate_kBs = msc.rate_kBs;
  } else {
    // Idle window: let sampler run ~same wall time as a short transfer.
    delay(50);
    r.traffic_ok = false;
  }

  shared.run = false;
  const uint32_t wait_t0 = millis();
  while (!shared.done && (millis() - wait_t0) < 5000) {
    delay(1);
  }
  if (!shared.done) {
    r.error = "sampler_timeout";
    r.duration_ms = millis() - t0;
    return r;
  }

  r.samples = shared.samples;
  r.duration_us = shared.duration_us;
  r.transitions_dm = shared.transitions_dm;
  r.transitions_dp = shared.transitions_dp;
  r.sample_dt_min_cy = (shared.dt_min == UINT32_MAX) ? 0 : shared.dt_min;
  r.sample_dt_max_cy = shared.dt_max;
  if (shared.samples > 1) {
    r.sample_dt_avg_cy =
        static_cast<uint32_t>(shared.dt_sum / (shared.samples - 1));
  }
  if (r.duration_us > 0) {
    r.rate_ksps = static_cast<uint32_t>(
        (static_cast<uint64_t>(r.samples) * 1000ULL) / r.duration_us);
  }
  if (r.cpu_mhz > 0 && r.sample_dt_max_cy >= r.sample_dt_min_cy) {
    const uint32_t pp_cy = r.sample_dt_max_cy - r.sample_dt_min_cy;
    r.jitter_pp_ns =
        static_cast<uint32_t>((static_cast<uint64_t>(pp_cy) * 1000ULL) /
                              r.cpu_mhz);
  }

  r.dut_after = deviceCount() > 0;
  r.hprt_after = USB_DWC.hprt_reg.val;
  r.prt_conn_after = usb_dwc_ll_hprt_get_conn_status(&USB_DWC);
  r.prt_ena_after = usb_dwc_ll_hprt_get_port_en(&USB_DWC);

  // Post health: short MSC or GET_DESCRIPTOR.
  if (r.dut_after) {
    const HostMscProbeResult post = runMscProbe(8, 0, 4);
    r.post_probe_ok = post.ok && post.pattern_ok;
  } else {
    r.post_probe_ok = false;
  }

  r.bus_stable = r.dut_after && r.prt_ena_after && r.post_probe_ok &&
                 r.prt_conn_after;
  // Peek-only is expected non-destructive; flag if bus died anyway.
  r.destructive = !r.bus_stable;
  // "Meaningful" = saw edges during concurrent traffic (not SE0-idle noise).
  const uint32_t edges = r.transitions_dm + r.transitions_dp;
  r.meaningful_edges = r.traffic_ok && edges >= 16;

  // FS needs ≥12 MS/s (≥12000 ksps); prefer 24+. Measured poll is far below.
  const bool rate_ok_for_fs = r.rate_ksps >= 24000;
  r.path_a_usable = false;
  if (r.destructive) {
    r.path_a_reason = "gpio_poll_coincided_with_bus_instability";
  } else if (!rate_ok_for_fs) {
    r.path_a_reason =
        "poll_rate_below_fs_nyquist_need_ge_24Msps_path_b_required";
  } else if (!r.meaningful_edges) {
    r.path_a_reason =
        "no_packet_grade_edges_while_otg_owns_internal_phy_pins";
  } else {
    r.path_a_reason =
        "edges_seen_but_not_a_validated_fs_packet_sniffer_prefer_path_b";
  }

  r.ok = true;
  r.duration_ms = millis() - t0;
  CaptureLog::instance().push(
      CapRole::Host, CapType::EnumInfo, r.destructive ? 1 : 0, r.rate_ksps,
      r.transitions_dm + r.transitions_dp, r.jitter_pp_ns, 0, "e11_gpio_sample");
  return r;
}

UsbHostNode::CtrlNegResult UsbHostNode::runCtrlNegSeries() {
  CtrlNegResult r;
  const uint32_t t0 = millis();
  if (!begin_ok_ || deviceCount() == 0) {
    r.error = begin_ok_ ? "no_dut" : "host_disabled";
    r.duration_ms = millis() - t0;
    return r;
  }

  auto run_case = [&](const char *name, bool expect_ok, uint8_t bm, uint8_t br,
                      uint16_t wv, uint16_t wi, uint8_t *data, uint16_t wl) {
    if (r.cases >= 8) {
      return;
    }
    CtrlCaseResult &c = r.detail[r.cases++];
    c.name = name;
    c.expect_ok = expect_ok;
    uint16_t actual = 0;
    c.ok = controlTransfer(bm, br, wv, wi, data, wl, &actual);
    c.actual = actual;
    c.host_err = g_usb.lastError();
    c.pass = (c.ok == expect_ok);
    if (c.pass) {
      r.passed++;
    }
    CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, c.pass ? 0 : 1,
                                actual, br, wv, 0, name);
  };

  uint8_t buf[64];
  memset(buf, 0, sizeof(buf));

  // Expectations tuned to TinyUSB/EspUsbHost lab MEASURED behavior (not USB-IF).
  // 1) Positive: GET_DESCRIPTOR(DEVICE)
  run_case("get_desc_dev", true, 0x80, 0x06, 0x0100, 0, buf, 18);
  // 2) GET_STATUS(device)
  run_case("get_status", true, 0x80, 0x00, 0, 0, buf, 2);
  // 3) Short GET_DESCRIPTOR (8 of 18)
  run_case("get_desc_short", true, 0x80, 0x06, 0x0100, 0, buf, 8);
  // 4) Invalid descriptor type — expect fail (string idx 0xFF often stubs ok)
  run_case("bad_desc_type", false, 0x80, 0x06, 0x7F00, 0, buf, 8);
  // 5) Invalid bRequest — expect fail
  run_case("bad_brequest", false, 0x80, 0xEE, 0, 0, buf, 2);
  // 6) Unimplemented vendor IN — expect fail/STALL
  run_case("vendor_unimpl", false, 0xC0, 0xAB, 0, 0, buf, 8);
  // 7) GET_INTERFACE iface 0 — TinyUSB returns altsetting
  run_case("get_iface", true, 0x81, 0x0A, 0, 0, buf, 1);
  // 8) Zero-length GET_DESCRIPTOR — fail/no-data
  run_case("get_desc_zlp", false, 0x80, 0x06, 0x0100, 0, buf, 0);

  r.ok = (r.passed == r.cases) && (r.cases > 0);
  if (!r.ok && r.error.length() == 0) {
    r.error = "cases_failed";
  }
  r.duration_ms = millis() - t0;
  return r;
}

UsbHostNode::HidObserveResult UsbHostNode::runHidObserve(uint32_t wait_ms,
                                                         bool clear) {
  HidObserveResult r;
  const uint32_t t0 = millis();
  if (!begin_ok_) {
    r.error = "host_disabled";
    r.duration_ms = millis() - t0;
    return r;
  }
  if (deviceCount() == 0) {
    r.error = "no_dut";
    r.duration_ms = millis() - t0;
    return r;
  }
  if (clear) {
    hid_got_ = false;
    hid_armed_ = true;
  }
  const uint32_t deadline = millis() + wait_ms;
  while (millis() < deadline) {
    if (hid_got_) {
      break;
    }
    delay(5);
  }
  hid_armed_ = false;
  r.got = hid_got_;
  r.keycode = hid_keycode_;
  r.ascii = hid_ascii_;
  r.modifiers = hid_modifiers_;
  r.iface = hid_iface_;
  r.report_len = hid_report_len_;
  memcpy(r.report, hid_report_, sizeof(r.report));
  r.report_count = hid_report_count_;
  r.age_ms = hid_last_ms_ ? (millis() - hid_last_ms_) : 0xffffffffu;
  r.ok = r.got && (r.keycode != 0 || r.ascii != 0 || r.report_len > 0);
  if (!r.ok && r.error.length() == 0) {
    r.error = "timeout_no_hid";
  }
  r.duration_ms = millis() - t0;
  return r;
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
bool UsbHostNode::writeBlocks(uint32_t, uint32_t, const uint8_t *) {
  return false;
}
HostMscProbeResult UsbHostNode::runMscWriteProbe(uint32_t, uint32_t, uint32_t,
                                                 bool) {
  HostMscProbeResult r;
  r.error = "host_disabled";
  last_probe_ = r;
  has_last_probe_ = true;
  return r;
}
void UsbHostNode::printNoDeviceHints() const {}
void UsbHostNode::clearDut() {}
bool UsbHostNode::fillDutSnapshot(DutInfoSnapshot &out) {
  memset(&out, 0, sizeof(out));
  return false;
}
bool UsbHostNode::controlTransfer(uint8_t, uint8_t, uint16_t, uint16_t,
                                  uint8_t *, uint16_t, uint16_t *) {
  return false;
}
UsbHostNode::AoaHandshakeResult UsbHostNode::runAoaHandshake(const char *,
                                                             bool) {
  AoaHandshakeResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::AoaBulkResult UsbHostNode::runAoaBulkEcho(const char *) {
  AoaBulkResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::AoaBulkResult UsbHostNode::runAoaBulkEcho(const uint8_t *, size_t) {
  AoaBulkResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::AoaBclResult UsbHostNode::runAoaBclRoundtrip(uint16_t, uint16_t,
                                                          bool, const uint8_t *,
                                                          size_t, bool) {
  AoaBclResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::EnumObserveResult UsbHostNode::runEnumObserve(bool) {
  EnumObserveResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::HidObserveResult UsbHostNode::runHidObserve(uint32_t, bool) {
  HidObserveResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::SuspendResumeResult UsbHostNode::runBusSuspend(uint32_t) {
  SuspendResumeResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::SuspendResumeResult UsbHostNode::runBusResume() {
  SuspendResumeResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::SuspendResumeResult UsbHostNode::runBusSuspendResume(uint32_t) {
  SuspendResumeResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::CtrlNegResult UsbHostNode::runCtrlNegSeries() {
  CtrlNegResult r;
  r.error = "host_disabled";
  return r;
}
UsbHostNode::L0FeasibilityResult UsbHostNode::runL0Feasibility() {
  L0FeasibilityResult r;
  r.path_a_sniffer_ok = false;
  r.path_a_reason = "host_disabled";
  r.error = "host_disabled";
  r.ok = true;
  return r;
}
UsbHostNode::E11GpioSampleResult
UsbHostNode::runE11GpioSample(uint32_t, uint32_t) {
  E11GpioSampleResult r;
  r.path_a_usable = false;
  r.path_a_reason = "host_disabled";
  r.error = "host_disabled";
  r.ok = true;
  return r;
}
bool UsbHostNode::releaseHostStack(String &err) {
  err = "host_disabled";
  return false;
}
UsbHostNode::FaultObserveResult
UsbHostNode::runFaultObserve(uint32_t, uint32_t, bool, bool) {
  FaultObserveResult r;
  r.error = "host_disabled";
  return r;
}
void UsbHostNode::noteHidReport(uint8_t, uint8_t, uint8_t, uint8_t,
                                const uint8_t *, size_t) {}

} // namespace usb_analyzer

#endif
