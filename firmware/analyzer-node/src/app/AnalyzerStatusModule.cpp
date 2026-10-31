#include "app/AnalyzerStatusModule.h"

#include "ProjectConfig.h"
#include "UiPages.h"
#include "build_id.h"
#include "capture/CaptureLog.h"
#include "device/UsbDeviceNode.h"
#include "host/UsbHostNode.h" // HostMscProbeResult
#include "protocol/DataPlane.h"
#include <cstring>
#include <esp_timer.h>
#include <usb_analyzer/Role.h>

namespace usb_analyzer {

void AnalyzerStatusModule::begin() {
  led_.begin(static_cast<uint8_t>(RGB_LED_PIN));
  CaptureLog::instance().setRoleName(roleName(role_));
}

void AnalyzerStatusModule::loop() {
  led_.loop();
  CaptureLog::instance().setRoleName(roleName(role_));
}

void AnalyzerStatusModule::setController(const String &host, uint16_t port) {
  controller_host_ = host;
  controller_port_ = port;
}

void AnalyzerStatusModule::appendStatusJson(String &json) {
  json += ",\"role\":\"";
  json += roleName(role_);
  json += "\",\"git\":\"";
  json += GIT_SHA;
  json += "\",\"controller\":\"";
  json += controller_host_;
  json += ":";
  json += String(controller_port_);
  json += "\"";

  // OTG / USB bus presence (role-dependent)
  json += ",\"otg\":{";
  if (role_ == Role::Host && host_) {
    json += "\"mode\":\"host\"";
    json += ",\"state\":\"";
    json += host_->otgState();
    json += "\",\"connected\":";
    json += host_->deviceCount() > 0 ? "true" : "false";
    json += ",\"devices\":";
    json += String(static_cast<unsigned>(host_->deviceCount()));
    json += ",\"meaning\":\"dut_on_otg\"";
  } else if (role_ == Role::Device && device_) {
    json += "\"mode\":\"device\"";
    json += ",\"state\":\"";
    json += device_->otgState();
    json += "\",\"connected\":";
    json += device_->otgMounted() ? "true" : "false";
    json += ",\"suspended\":";
    json += device_->otgSuspended() ? "true" : "false";
    json += ",\"meaning\":\"gadget_mounted_on_usb_host\"";
  } else {
    json += "\"mode\":\"none\",\"state\":\"n/a\",\"connected\":false";
  }
  json += "}";

  if (role_ == Role::Host && host_) {
    host_->appendStatusJson(json);
  }
  if (role_ == Role::Device && device_) {
    device_->appendStatusJson(json);
  }

  if (data_) {
    json += ",\"dataPlane\":\"";
    json += data_->state();
    json += "\",\"txBytes\":";
    json += String(data_->txBytes());
    json += ",\"rxBytes\":";
    json += String(data_->rxBytes());
    json += ",\"gaps\":";
    json += String(data_->gaps());
    json += ",\"rateKBs\":";
    json += String(data_->rateKBs());
    json += ",\"proxyHits\":";
    json += String(data_->proxyHits());
    json += ",\"proxyFail\":";
    json += String(data_->proxyFail());
    json += ",\"lastPattern\":\"";
    json += data_->lastPattern();
    json += "\"";
    if (data_->hasLastResult()) {
      json += ",\"integrityMatch\":";
      json += data_->lastMatch() ? "true" : "false";
      const IntegrityResult &lr = data_->lastResult();
      json += ",\"lastIntegrity\":{";
      json += "\"ok\":";
      json += lr.ok ? "true" : "false";
      json += ",\"peer\":\"";
      json += data_->lastPeer();
      json += "\",\"frames_sent\":";
      json += String(lr.frames_sent);
      json += ",\"frames_acked\":";
      json += String(lr.frames_acked);
      json += ",\"frames_fail\":";
      json += String(lr.frames_fail);
      json += ",\"bytes_tx\":";
      json += String(lr.bytes_tx);
      json += ",\"duration_ms\":";
      json += String(lr.duration_ms);
      json += ",\"rate_kBs\":";
      json += String(data_->rateKBs());
      json += ",\"rtt_us\":{";
      json += "\"min\":" + String(lr.rtt_min_us);
      json += ",\"avg\":" + String(lr.rtt_avg_us);
      json += ",\"p50\":" + String(lr.rtt_p50_us);
      json += ",\"p95\":" + String(lr.rtt_p95_us);
      json += ",\"max\":" + String(lr.rtt_max_us);
      json += "}";
      if (lr.error.length()) {
        json += ",\"err\":\"" + lr.error + "\"";
      }
      json += "}";
    }
  } else {
    json += ",\"dataPlane\":\"offline\"";
  }

  led_.appendStatusJson(json);
  CaptureLog::instance().appendStatusJson(json);
}

void AnalyzerStatusModule::handleLed() {
  if (!server_) {
    return;
  }
  if (server_->method() != HTTP_POST && server_->method() != HTTP_GET) {
    server_->send(405, F("text/plain"), F("GET/POST"));
    return;
  }

  String color = server_->arg("color");
  String identify = server_->arg("identify");
  const String body = server_->arg("plain");
  if (color.isEmpty() && body.indexOf("\"color\"") >= 0) {
    const int p = body.indexOf("\"color\"");
    const int q1 = body.indexOf('"', body.indexOf(':', p) + 1);
    const int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1) {
      color = body.substring(q1 + 1, q2);
    }
  }
  if (identify.isEmpty() &&
      (body.indexOf("\"identify\":true") >= 0 ||
       body.indexOf("\"identify\":1") >= 0 || body == "identify")) {
    identify = "1";
  }

  bool ok = false;
  String err;
  if (identify == "1" || identify == "true" ||
      color.equalsIgnoreCase("identify")) {
    uint32_t ms = 5000;
    if (server_->hasArg("ms")) {
      ms = static_cast<uint32_t>(server_->arg("ms").toInt());
    }
    led_.identify(ms);
    ok = true;
  } else if (!color.isEmpty()) {
    ok = led_.setColorName(color);
    if (!ok) {
      err = "unknown_color";
    }
  } else {
    err = "color_or_identify_required";
  }

  String j = "{";
  j += "\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"color\":\"";
  j += led_.colorName();
  j += "\",\"identify\":";
  j += led_.identifying() ? "true" : "false";
  if (err.length()) {
    j += ",\"err\":\"";
    j += err;
    j += "\"";
  }
  j += "}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(ok ? 200 : 400, F("application/json"), j);
}

void AnalyzerStatusModule::handleIntegrity() {
  if (!server_ || !data_) {
    return;
  }
  if (server_->method() != HTTP_POST && server_->method() != HTTP_GET) {
    server_->send(405, F("text/plain"), F("GET/POST"));
    return;
  }
  String body = server_->arg("plain");
  String peer = server_->arg("peer");
  uint32_t frames = 64;
  uint32_t frame_bytes = 1024;
  uint32_t window = 8; // Gate-4 default: windowed bulk (override with window=1)
  if (server_->hasArg("frames")) {
    frames = static_cast<uint32_t>(server_->arg("frames").toInt());
  }
  if (server_->hasArg("frame_bytes")) {
    frame_bytes = static_cast<uint32_t>(server_->arg("frame_bytes").toInt());
  }
  if (server_->hasArg("window")) {
    window = static_cast<uint32_t>(server_->arg("window").toInt());
  }
  // optional JSON body
  const int p = body.indexOf("\"peer\"");
  if (peer.isEmpty() && p >= 0) {
    const int q1 = body.indexOf('"', body.indexOf(':', p) + 1);
    const int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1) {
      peer = body.substring(q1 + 1, q2);
    }
  }
  if (frames == 0) {
    frames = 64;
  }
  if (frame_bytes == 0) {
    frame_bytes = 1024;
  }
  if (peer.isEmpty()) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"peer_required\"}"));
    return;
  }

  const IntegrityResult r =
      data_->runBulk(peer, frames, frame_bytes, window == 0 ? 1 : window);
  String j = "{";
  j += "\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"peer\":\"" + peer + "\"";
  j += ",\"mode\":\"" + r.mode + "\"";
  j += ",\"window\":" + String(r.window);
  j += ",\"frames_sent\":" + String(r.frames_sent);
  j += ",\"frames_acked\":" + String(r.frames_acked);
  j += ",\"frames_fail\":" + String(r.frames_fail);
  j += ",\"bytes_tx\":" + String(r.bytes_tx);
  j += ",\"bytes_rx\":" + String(r.bytes_rx);
  j += ",\"duration_ms\":" + String(r.duration_ms);
  if (r.duration_ms > 0) {
    const uint32_t kbs = static_cast<uint32_t>(
        (static_cast<uint64_t>(r.bytes_tx) * 1000ull / r.duration_ms) / 1024ull);
    j += ",\"rate_kBs\":" + String(kbs);
  }
  j += ",\"rtt_us\":{";
  j += "\"min\":" + String(r.rtt_min_us);
  j += ",\"avg\":" + String(r.rtt_avg_us);
  j += ",\"p50\":" + String(r.rtt_p50_us);
  j += ",\"p95\":" + String(r.rtt_p95_us);
  j += ",\"max\":" + String(r.rtt_max_us);
  j += "}";
  j += ",\"path\":\"tcp_wifi_esp_esp\"";
  j += ",\"note\":\"not_usb_mitm\"";
  if (r.error.length()) {
    j += ",\"err\":\"" + r.error + "\"";
  }
  j += "}";
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleUsbProxy() {
  if (!server_) {
    return;
  }
  if (!device_ || role_ != Role::Device) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"device_role_required\"}"));
    return;
  }
  String peer = server_->arg("peer");
  const String en = server_->arg("enable");
  if (en == "0" || en == "false" || en == "off") {
    device_->setProxy(nullptr, "");
    server_->send(200, F("application/json"),
                  F("{\"ok\":true,\"proxy\":false}"));
    return;
  }
  if (peer.isEmpty()) {
    peer = "192.168.178.88";
  }
  if (!data_) {
    server_->send(500, F("application/json"),
                  F("{\"ok\":false,\"err\":\"no_dataplane\"}"));
    return;
  }
  device_->setProxy(data_, peer);
  String j = "{\"ok\":true,\"proxy\":true,\"peer\":\"";
  j += peer;
  j += "\",\"note\":\"MITM wiring: Device OTG→PC, Host OTG→DUT; not OTG-loop\"";
  j += ",\"identity\":\"async_apply_dut_snapshot_soft_reenum\"";
  j += ",\"claim\":\"gadget_identity_from_dut_snapshot\"}";
  server_->send(200, F("application/json"), j);
}

void AnalyzerStatusModule::handleUsbHostProbe() {
  if (!server_) {
    return;
  }
  if (server_->method() != HTTP_POST && server_->method() != HTTP_GET) {
    server_->send(405, F("text/plain"), F("GET/POST"));
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  uint32_t blocks = 64;
  uint32_t lba = 0;
  uint32_t chunk = 1;
  String dir = server_->arg("dir");
  if (server_->hasArg("blocks")) {
    blocks = static_cast<uint32_t>(server_->arg("blocks").toInt());
  }
  if (server_->hasArg("lba")) {
    lba = static_cast<uint32_t>(server_->arg("lba").toInt());
  }
  if (server_->hasArg("chunk")) {
    chunk = static_cast<uint32_t>(server_->arg("chunk").toInt());
  }
  if (blocks == 0) {
    blocks = 64;
  }
  dir.toLowerCase();
  const bool do_write = (dir == "write" || dir == "w" || dir == "out");

  const HostMscProbeResult r =
      do_write ? host_->runMscWriteProbe(blocks, lba, chunk, true)
               : host_->runMscProbe(blocks, lba, chunk);
  String j = "{";
  j += "\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"msc_ready\":";
  j += r.msc_ready ? "true" : "false";
  j += ",\"pattern_ok\":";
  j += r.pattern_ok ? "true" : "false";
  j += ",\"pattern_fail_bytes\":" + String(r.pattern_fail_bytes);
  j += ",\"dut\":\"" + r.dut_id + "\"";
  j += ",\"inquiry\":{";
  j += "\"vendor\":\"" + r.inquiry_vendor + "\"";
  j += ",\"product\":\"" + r.inquiry_product + "\"";
  j += ",\"revision\":\"" + r.inquiry_revision + "\"";
  j += "}";
  j += ",\"capacity_blocks\":" + String(static_cast<unsigned long>(r.capacity_blocks));
  j += ",\"block_size\":" + String(r.block_size);
  j += ",\"chunk\":" + String(r.chunk);
  j += ",\"transfers\":" + String(r.transfers);
  j += ",\"blocks_read\":" + String(r.blocks_read);
  j += ",\"bytes_read\":" + String(r.bytes_read);
  j += ",\"duration_ms\":" + String(r.duration_ms);
  j += ",\"rate_kBs\":" + String(r.rate_kBs);
  j += ",\"rtt_us\":{";
  j += "\"min\":" + String(r.rtt_min_us);
  j += ",\"avg\":" + String(r.rtt_avg_us);
  j += ",\"p50\":" + String(r.rtt_p50_us);
  j += ",\"p95\":" + String(r.rtt_p95_us);
  j += ",\"max\":" + String(r.rtt_max_us);
  j += "}";
  j += ",\"dir\":\"";
  j += do_write ? "write" : "read";
  j += "\"";
  j += ",\"path\":\"";
  j += do_write ? "usb_otg_host_write_device_msc" : "usb_otg_host_to_device_msc";
  j += "\"";
  j += ",\"note\":\"direct_otg_cable_not_wifi_mitm\"";
  if (r.error.length()) {
    j += ",\"err\":\"" + r.error + "\"";
  }
  j += "}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleUsbHostRead() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  uint32_t lba = 0;
  uint32_t blocks = 4;
  if (server_->hasArg("lba")) {
    lba = static_cast<uint32_t>(server_->arg("lba").toInt());
  }
  if (server_->hasArg("blocks")) {
    blocks = static_cast<uint32_t>(server_->arg("blocks").toInt());
  }
  if (blocks == 0 || blocks > 16) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"blocks_1_16\"}"));
    return;
  }
  uint8_t *buf = static_cast<uint8_t *>(malloc(blocks * 512u));
  if (!buf) {
    server_->send(500, F("application/json"),
                  F("{\"ok\":false,\"err\":\"oom\"}"));
    return;
  }
  const uint32_t t0 = millis();
  const bool ok = host_->readBlocks(lba, blocks, buf);
  const uint32_t dur = millis() - t0;
  String j = "{\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"lba\":";
  j += String(lba);
  j += ",\"blocks\":";
  j += String(blocks);
  j += ",\"bytes\":";
  j += String(blocks * 512u);
  j += ",\"duration_ms\":";
  j += String(dur);
  if (ok) {
    char crcbuf[12];
    snprintf(crcbuf, sizeof(crcbuf), "%08x",
             DataPlane::crc32(buf, blocks * 512u));
    j += ",\"crc32\":\"";
    j += crcbuf;
    j += "\",\"hex\":\"";
    const uint32_t nbytes = blocks * 512u;
    for (uint32_t i = 0; i < nbytes; i++) {
      char hx[3];
      snprintf(hx, sizeof(hx), "%02x", buf[i]);
      j += hx;
    }
    j += "\"";
  } else {
    j += ",\"err\":\"msc_read_fail\"";
  }
  j += ",\"path\":\"usb_otg_host_dut_raw\"";
  j += ",\"note\":\"direct_otg_not_wifi_mitm\"}";
  free(buf);
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleUsbProxySelftest() {
  if (!server_) {
    return;
  }
  if (!device_ || role_ != Role::Device) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"device_role_required\"}"));
    return;
  }
  if (!device_->proxyEnabled()) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"proxy_off\"}"));
    return;
  }
  uint32_t lba = 0;
  uint32_t blocks = 4;
  if (server_->hasArg("lba")) {
    lba = static_cast<uint32_t>(server_->arg("lba").toInt());
  }
  if (server_->hasArg("blocks")) {
    blocks = static_cast<uint32_t>(server_->arg("blocks").toInt());
  }
  if (blocks == 0 || blocks > 16) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"blocks_1_16\"}"));
    return;
  }
  uint8_t *buf = static_cast<uint8_t *>(malloc(blocks * 512u));
  if (!buf) {
    server_->send(500, F("application/json"),
                  F("{\"ok\":false,\"err\":\"oom\"}"));
    return;
  }
  String err;
  const uint32_t t0 = millis();
  const bool ok = device_->selftestProxyRead(lba, blocks, buf, &err);
  const uint32_t dur = millis() - t0;
  String j = "{\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"lba\":";
  j += String(lba);
  j += ",\"blocks\":";
  j += String(blocks);
  j += ",\"bytes\":";
  j += String(blocks * 512u);
  j += ",\"duration_ms\":";
  j += String(dur);
  j += ",\"proxyHits\":";
  j += String(device_->proxyHits());
  j += ",\"proxyFails\":";
  j += String(device_->proxyFails());
  if (ok) {
    char crcbuf[12];
    snprintf(crcbuf, sizeof(crcbuf), "%08x",
             DataPlane::crc32(buf, blocks * 512u));
    j += ",\"crc32\":\"";
    j += crcbuf;
    j += "\",\"hex\":\"";
    const uint32_t nbytes = blocks * 512u;
    for (uint32_t i = 0; i < nbytes; i++) {
      char hx[3];
      snprintf(hx, sizeof(hx), "%02x", buf[i]);
      j += hx;
    }
    j += "\"";
  } else {
    j += ",\"err\":\"";
    j += err.length() ? err : "fail";
    j += "\"";
  }
  j += ",\"path\":\"device_proxy_to_host_dut\"";
  j += ",\"claim\":\"msc_bulk_proxy_selftest\"";
  j += ",\"note\":\"not_pc_path_not_transparent_mitm\"}";
  free(buf);
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::registerRoutes(WebServer &server) {
  server_ = &server;
  server.on("/analyzer", HTTP_GET, [&server]() {
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    server.sendHeader(F("Cache-Control"), F("no-store"));
    server.send_P(200, "text/html", reinterpret_cast<PGM_P>(PAGE_MAIN_GZ),
                  PAGE_MAIN_GZ_LEN);
  });
  server.on("/api/lab/integrity", HTTP_ANY, [this]() { handleIntegrity(); });
  server.on("/api/lab/usb-host-probe", HTTP_ANY,
            [this]() { handleUsbHostProbe(); });
  server.on("/api/lab/usb-host-read", HTTP_ANY,
            [this]() { handleUsbHostRead(); });
  server.on("/api/lab/usb-proxy", HTTP_ANY, [this]() { handleUsbProxy(); });
  server.on("/api/lab/usb-proxy-selftest", HTTP_ANY,
            [this]() { handleUsbProxySelftest(); });
  server.on("/api/lab/usb-ctrl-forward", HTTP_ANY,
            [this]() { handleUsbCtrlForward(); });
  server.on("/api/lab/sync-marker", HTTP_ANY, [this]() { handleSyncMarker(); });
  server.on("/api/capture", HTTP_GET, [this]() { handleCapture(); });
  server.on("/api/led", HTTP_ANY, [this]() { handleLed(); });
}

void AnalyzerStatusModule::handleSyncMarker() {
  if (!server_) {
    return;
  }
  uint32_t nonce = static_cast<uint32_t>(millis());
  if (server_->hasArg("nonce")) {
    nonce = static_cast<uint32_t>(server_->arg("nonce").toInt());
  }
  const uint32_t ts = static_cast<uint32_t>(esp_timer_get_time());
  CaptureLog::instance().push(CapRole::Plane, CapType::SyncMarker, 0, 0, nonce,
                              ts, 0, "lab_sync");
  String j = "{\"ok\":true,\"nonce\":";
  j += String(nonce);
  j += ",\"timestamp_us\":";
  j += String(ts);
  j += ",\"role\":\"";
  j += roleName(role_);
  j += "\",\"claim\":\"cross_node_offset_estimate\"";
  j += ",\"note\":\"not_hard_us_lock\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(200, F("application/json"), j);
}

void AnalyzerStatusModule::handleUsbCtrlForward() {
  if (!server_) {
    return;
  }
  if (!device_ || role_ != Role::Device) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"device_role_required\"}"));
    return;
  }
  if (!data_) {
    server_->send(500, F("application/json"),
                  F("{\"ok\":false,\"err\":\"no_dataplane\"}"));
    return;
  }
  String peer = server_->arg("peer");
  if (peer.isEmpty()) {
    peer = "192.168.178.88";
  }
  String kind = server_->arg("desc");
  kind.toLowerCase();
  if (kind.isEmpty()) {
    kind = "device";
  }
  uint8_t str_index = 2;
  if (server_->hasArg("index")) {
    str_index = static_cast<uint8_t>(server_->arg("index").toInt());
  }

  const uint8_t bm = 0x80;
  const uint8_t br = 0x06; // GET_DESCRIPTOR
  uint16_t wv = 0x0100;    // DEVICE
  uint16_t wi = 0;
  uint16_t wl = 18;
  if (kind == "string") {
    if (str_index < 1 || str_index > 3) {
      server_->send(400, F("application/json"),
                    F("{\"ok\":false,\"err\":\"string_index_1_3\"}"));
      return;
    }
    wv = static_cast<uint16_t>(0x0300u | str_index);
    wi = 0x0409; // en-US
    wl = 64;
  } else if (kind != "device") {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"desc_device_or_string\"}"));
    return;
  }

  uint8_t buf[64];
  memset(buf, 0, sizeof(buf));
  uint16_t actual = 0;
  String err;
  const uint32_t t0 = millis();
  const bool ok = data_->proxyControlTransfer(peer, bm, br, wv, wi, buf, wl,
                                              &actual, &err);
  const uint32_t dur = millis() - t0;

  bool match = false;
  char decoded[48] = {0};
  if (ok && kind == "device" && actual >= 18) {
    uint8_t local[18];
    if (device_->copyLocalDeviceDescriptor(local)) {
      match = (memcmp(buf, local, 18) == 0);
    }
  } else if (ok && kind == "string" && actual >= 2 && buf[1] == 0x03) {
    // USB string: UTF-16LE after 2-byte header → ASCII decode
    const uint8_t nchars = (buf[0] > 2) ? static_cast<uint8_t>((buf[0] - 2) / 2) : 0;
    size_t di = 0;
    for (uint8_t i = 0; i < nchars && di + 1 < sizeof(decoded); i++) {
      decoded[di++] = static_cast<char>(buf[2 + i * 2]);
    }
    decoded[di] = '\0';
    char local[48];
    if (device_->localStringAscii(str_index, local, sizeof(local))) {
      match = (strcmp(decoded, local) == 0);
    }
  }

  String j = "{\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"peer\":\"";
  j += peer;
  j += "\",\"desc\":\"";
  j += kind;
  j += "\",\"bmRequestType\":128,\"bRequest\":6,\"wValue\":";
  j += String(wv);
  j += ",\"wIndex\":";
  j += String(wi);
  j += ",\"wLength\":";
  j += String(wl);
  j += ",\"actual\":";
  j += String(actual);
  j += ",\"duration_ms\":";
  j += String(dur);
  j += ",\"match_local_desc\":";
  j += match ? "true" : "false";
  if (kind == "string" && decoded[0]) {
    j += ",\"decoded\":\"";
    j += decoded;
    j += "\"";
  }
  if (ok && actual > 0) {
    char crcbuf[12];
    snprintf(crcbuf, sizeof(crcbuf), "%08x", DataPlane::crc32(buf, actual));
    j += ",\"crc32\":\"";
    j += crcbuf;
    j += "\",\"hex\":\"";
    for (uint16_t i = 0; i < actual; i++) {
      char hx[3];
      snprintf(hx, sizeof(hx), "%02x", buf[i]);
      j += hx;
    }
    j += "\"";
  } else if (err.length()) {
    j += ",\"err\":\"";
    j += err;
    j += "\"";
  }
  j += ",\"path\":\"ep0_get_desc_forward_lab_loop\"";
  j += ",\"claim\":\"ep0_get_desc_forward_lab_loop\"";
  j += ",\"note\":\"not_usb_control_mitm\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleCapture() {
  if (!server_) {
    return;
  }
  uint32_t since = 0;
  uint32_t limit = 64;
  if (server_->hasArg("since")) {
    since = static_cast<uint32_t>(server_->arg("since").toInt());
  }
  if (server_->hasArg("limit")) {
    limit = static_cast<uint32_t>(server_->arg("limit").toInt());
  }
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(200, F("application/json"),
                CaptureLog::instance().buildBatchJson(since, limit));
}

} // namespace usb_analyzer
