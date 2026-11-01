#include "app/AnalyzerStatusModule.h"

#include "ProjectConfig.h"
#include "UiPages.h"
#include "build_id.h"
#include "capture/CaptureLog.h"
#include "device/UsbDeviceNode.h"
#include "host/UsbHostNode.h" // HostMscProbeResult
#include "l0/GpioBitbangTiming.h"
#include "l0/PathBGpioLab.h"
#include "l0/PathBGpioRx.h"
#include "l0/WireStimulus.h"
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
  bool pipeline = false;
  if (server_->hasArg("frames")) {
    frames = static_cast<uint32_t>(server_->arg("frames").toInt());
  }
  if (server_->hasArg("frame_bytes")) {
    frame_bytes = static_cast<uint32_t>(server_->arg("frame_bytes").toInt());
  }
  if (server_->hasArg("window")) {
    window = static_cast<uint32_t>(server_->arg("window").toInt());
  }
  if (server_->hasArg("pipeline")) {
    const String p = server_->arg("pipeline");
    pipeline = (p == "1" || p == "true" || p == "on");
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

  const IntegrityResult r = data_->runBulk(
      peer, frames, frame_bytes, window == 0 ? 1 : window, pipeline);
  String j = "{";
  j += "\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"peer\":\"" + peer + "\"";
  j += ",\"mode\":\"" + r.mode + "\"";
  j += ",\"window\":" + String(r.window);
  j += ",\"pipeline\":";
  j += r.pipeline ? "true" : "false";
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
  bool skip_id = false;
  if (server_->hasArg("identity")) {
    const String id = server_->arg("identity");
    skip_id = (id == "0" || id == "false" || id == "off" || id == "skip");
  }
  if (server_->hasArg("reenum")) {
    const String r = server_->arg("reenum");
    if (r == "0" || r == "false" || r == "off") {
      skip_id = true;
    }
  }
  device_->setProxy(data_, peer, skip_id);
  String j = "{\"ok\":true,\"proxy\":true,\"peer\":\"";
  j += peer;
  j += "\",\"skipIdentity\":";
  j += skip_id ? "true" : "false";
  j += ",\"note\":\"";
  j += skip_id ? "otg_loop_lab_no_identity_reenum"
               : "MITM wiring: Device OTG→PC, Host OTG→DUT; not OTG-loop";
  j += "\",\"identity\":\"";
  j += skip_id ? "skipped" : "async_apply_dut_snapshot_soft_reenum";
  j += "\",\"claim\":\"gadget_identity_from_dut_snapshot\"}";
  server_->send(200, F("application/json"), j);
}

void AnalyzerStatusModule::handleProxySource() {
  if (!server_ || !data_) {
    return;
  }
  if (role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  String mode = server_->hasArg("mode") ? server_->arg("mode") : "";
  if (mode.length() == 0 && server_->hasArg("source")) {
    mode = server_->arg("source");
  }
  if (mode == "pattern" || mode == "lab") {
    data_->setMscProxySourcePattern(true);
  } else if (mode == "dut" || mode == "usb") {
    data_->setMscProxySourcePattern(false);
  }
  String j = "{\"ok\":true,\"source\":\"";
  j += data_->mscProxySourcePattern() ? "pattern" : "dut";
  j += "\",\"claim\":\"msc_proxy_source_lab\"";
  j += ",\"note\":\"pattern_avoids_otg_loop_proxy_recursion\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
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
  server.on("/api/lab/proxy-source", HTTP_ANY,
            [this]() { handleProxySource(); });
  server.on("/api/lab/usb-ctrl-forward", HTTP_ANY,
            [this]() { handleUsbCtrlForward(); });
  server.on("/api/lab/aoa-respond", HTTP_ANY, [this]() { handleAoaRespond(); });
  server.on("/api/lab/aoa-handshake", HTTP_ANY,
            [this]() { handleAoaHandshake(); });
  server.on("/api/lab/aoa-bulk", HTTP_ANY, [this]() { handleAoaBulk(); });
  server.on("/api/lab/aoa-bcl", HTTP_ANY, [this]() { handleAoaBcl(); });
  server.on("/api/lab/usb-enum-observe", HTTP_ANY,
            [this]() { handleUsbEnumObserve(); });
  server.on("/api/lab/hid-observe", HTTP_ANY, [this]() { handleHidObserve(); });
  server.on("/api/lab/hid-inject", HTTP_ANY, [this]() { handleHidInject(); });
  server.on("/api/lab/soft-reenum", HTTP_ANY, [this]() { handleSoftReenum(); });
  server.on("/api/lab/usb-suspend-resume", HTTP_ANY,
            [this]() { handleUsbSuspendResume(); });
  server.on("/api/lab/usb-ctrl-neg", HTTP_ANY, [this]() { handleUsbCtrlNeg(); });
  server.on("/api/lab/l0-feasibility", HTTP_ANY,
            [this]() { handleL0Feasibility(); });
  server.on("/api/lab/e11-gpio-sample", HTTP_ANY,
            [this]() { handleE11GpioSample(); });
  server.on("/api/lab/path-b-gpio", HTTP_ANY,
            [this]() { handlePathBGpio(); });
  server.on("/api/lab/path-b-gpio-rx", HTTP_ANY,
            [this]() { handlePathBGpioRx(); });
  server.on("/api/lab/gpio-calibrate", HTTP_ANY,
            [this]() { handleGpioCalibrate(); });
  server.on("/api/lab/wire-stimulus", HTTP_ANY,
            [this]() { handleWireStimulus(); });
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

void AnalyzerStatusModule::handleAoaRespond() {
  if (!server_) {
    return;
  }
  if (!device_ || role_ != Role::Device) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"device_role_required\"}"));
    return;
  }
  const String en = server_->arg("enable");
  bool on = true;
  if (en == "0" || en == "false" || en == "off") {
    on = false;
  } else if (en == "1" || en == "true" || en == "on" || en.isEmpty()) {
    on = true;
  }
  bool reenum = true;
  if (server_->hasArg("reenum")) {
    const String r = server_->arg("reenum");
    reenum = !(r == "0" || r == "false" || r == "off");
  }
  // Proxy OFF recommended — AOA lab is control path on OTG loop.
  if (on && device_->proxyEnabled()) {
    device_->setProxy(nullptr, "");
  }
  device_->setAoaRespond(on, reenum);
  String j = "{\"ok\":true,\"respond\":";
  j += on ? "true" : "false";
  j += ",\"reenumOnStart\":";
  j += reenum ? "true" : "false";
  j += ",\"claim\":\"aoa_control_handshake_lab\"";
  j += ",\"note\":\"not_hu_wire_not_bcl_not_accessory_bulk\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(200, F("application/json"), j);
}

void AnalyzerStatusModule::handleAoaHandshake() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  String profile = server_->arg("profile");
  if (profile.isEmpty()) {
    profile = "lab";
  }
  bool wait_reenum = true;
  if (server_->hasArg("wait_reenum")) {
    const String w = server_->arg("wait_reenum");
    wait_reenum = !(w == "0" || w == "false" || w == "off");
  }
  const UsbHostNode::AoaHandshakeResult r =
      host_->runAoaHandshake(profile.c_str(), wait_reenum);
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"profile\":\"";
  j += r.profile;
  j += "\",\"protocol\":";
  j += String(r.protocol);
  j += ",\"protocolOk\":";
  j += r.protocol_ok ? "true" : "false";
  j += ",\"stringsOk\":";
  j += String(r.strings_ok);
  j += ",\"startOk\":";
  j += r.start_ok ? "true" : "false";
  j += ",\"reenumOk\":";
  j += r.reenum_ok ? "true" : "false";
  j += ",\"dutBefore\":\"";
  {
    char id[12];
    snprintf(id, sizeof(id), "%04x:%04x", r.dut_vid_before, r.dut_pid_before);
    j += id;
  }
  j += "\",\"dutAfter\":\"";
  {
    char id[12];
    snprintf(id, sizeof(id), "%04x:%04x", r.dut_vid_after, r.dut_pid_after);
    j += id;
  }
  j += "\",\"duration_ms\":";
  j += String(r.duration_ms);
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"aoa_control_handshake_lab\"";
  j += ",\"note\":\"not_hu_wire_not_bcl_not_accessory_bulk\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleAoaBulk() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  String payload = server_->arg("payload");
  if (payload.isEmpty()) {
    payload = "AOA-LAB";
  }
  const UsbHostNode::AoaBulkResult r = host_->runAoaBulkEcho(payload.c_str());
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"opened\":";
  j += r.opened ? "true" : "false";
  j += ",\"epOut\":";
  j += String(r.ep_out);
  j += ",\"epIn\":";
  j += String(r.ep_in);
  j += ",\"pktOut\":";
  j += String(r.pkt_out);
  j += ",\"pktIn\":";
  j += String(r.pkt_in);
  j += ",\"tx\":";
  j += String(r.tx);
  j += ",\"rx\":";
  j += String(r.rx);
  j += ",\"match\":";
  j += r.match ? "true" : "false";
  j += ",\"duration_ms\":";
  j += String(r.duration_ms);
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"aoa_bulk_echo_lab\"";
  j += ",\"note\":\"not_bcl_not_hu_wire\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleAoaBcl() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  uint16_t cmd = 4; // HANDSHAKE
  uint16_t dest = 0x0FA4;
  bool send_init = true;
  if (server_->hasArg("cmd")) {
    cmd = static_cast<uint16_t>(server_->arg("cmd").toInt());
  }
  if (server_->hasArg("dest")) {
    dest = static_cast<uint16_t>(strtoul(server_->arg("dest").c_str(), nullptr, 0));
  }
  if (server_->hasArg("init")) {
    const String i = server_->arg("init");
    send_init = !(i == "0" || i == "false" || i == "off");
  }
  // Optional plhex= (even empty) → explicit payload; omit → synthetic default.
  uint8_t pl_buf[56];
  size_t pl_len = 0;
  bool explicit_pl = false;
  if (server_->hasArg("plhex")) {
    explicit_pl = true;
    const String hx = server_->arg("plhex");
    for (unsigned i = 0; i + 1 < static_cast<unsigned>(hx.length()) && pl_len < sizeof(pl_buf);
         i += 2) {
      char a = hx.charAt(i);
      char b = hx.charAt(i + 1);
      auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9')
          return c - '0';
        if (c >= 'a' && c <= 'f')
          return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
          return c - 'A' + 10;
        return -1;
      };
      const int hi = nib(a);
      const int lo = nib(b);
      if (hi < 0 || lo < 0) {
        server_->send(400, F("application/json"),
                      F("{\"ok\":false,\"err\":\"bad_plhex\"}"));
        return;
      }
      pl_buf[pl_len++] = static_cast<uint8_t>((hi << 4) | lo);
    }
    if ((hx.length() % 2) != 0) {
      server_->send(400, F("application/json"),
                    F("{\"ok\":false,\"err\":\"odd_plhex\"}"));
      return;
    }
  }
  const UsbHostNode::AoaBclResult r = host_->runAoaBclRoundtrip(
      cmd, dest, send_init, explicit_pl ? pl_buf : nullptr, pl_len, explicit_pl);
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"initOk\":";
  j += r.init_ok ? "true" : "false";
  j += ",\"frameOk\":";
  j += r.frame_ok ? "true" : "false";
  j += ",\"parseOk\":";
  j += r.parse_ok ? "true" : "false";
  j += ",\"cmd\":";
  j += String(r.cmd);
  j += ",\"src\":";
  j += String(r.src);
  j += ",\"dest\":";
  j += String(r.dest);
  j += ",\"dataLen\":";
  j += String(r.data_len);
  j += ",\"tx\":";
  j += String(r.tx);
  j += ",\"rx\":";
  j += String(r.rx);
  j += ",\"duration_ms\":";
  j += String(r.duration_ms);
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"lab_bcl_on_aoa_bulk\"";
  j += ",\"note\":\"desk_header_fact_not_hu_wire\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleUsbEnumObserve() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  bool ep0 = true;
  if (server_->hasArg("ep0")) {
    const String v = server_->arg("ep0");
    ep0 = !(v == "0" || v == "false" || v == "off");
  }
  const UsbHostNode::EnumObserveResult r = host_->runEnumObserve(ep0);
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"dutPresent\":";
  j += r.dut_present ? "true" : "false";
  j += ",\"address\":";
  j += String(r.address);
  j += ",\"configurationValue\":";
  j += String(r.configuration_value);
  j += ",\"interfaces\":";
  j += String(r.iface_count);
  j += ",\"hidIface\":";
  j += r.hid_iface ? "true" : "false";
  j += ",\"getDescOk\":";
  j += r.get_desc_ok ? "true" : "false";
  j += ",\"getConfigOk\":";
  j += r.get_config_ok ? "true" : "false";
  j += ",\"getConfigMatch\":";
  j += r.get_config_match ? "true" : "false";
  j += ",\"getConfigByte\":";
  j += String(r.get_config_byte);
  j += ",\"duration_ms\":";
  j += String(r.duration_ms);
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"usb_enum_outcome_lab\"";
  j += ",\"note\":\"post_enum_stack_outcome_not_setup_mitm_not_l0\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleHidObserve() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  uint32_t wait_ms = 2000;
  bool clear = true;
  if (server_->hasArg("wait_ms")) {
    wait_ms = static_cast<uint32_t>(server_->arg("wait_ms").toInt());
  }
  if (server_->hasArg("clear")) {
    const String v = server_->arg("clear");
    clear = !(v == "0" || v == "false" || v == "off");
  }
  const UsbHostNode::HidObserveResult r = host_->runHidObserve(wait_ms, clear);
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"got\":";
  j += r.got ? "true" : "false";
  j += ",\"keycode\":";
  j += String(r.keycode);
  j += ",\"ascii\":";
  j += String(r.ascii);
  j += ",\"modifiers\":";
  j += String(r.modifiers);
  j += ",\"iface\":";
  j += String(r.iface);
  j += ",\"reportLen\":";
  j += String(r.report_len);
  j += ",\"reportHex\":\"";
  for (uint8_t i = 0; i < r.report_len; i++) {
    char hx[3];
    snprintf(hx, sizeof(hx), "%02x", r.report[i]);
    j += hx;
  }
  j += "\",\"age_ms\":";
  j += String(r.age_ms);
  j += ",\"reports\":";
  j += String(r.report_count);
  j += ",\"duration_ms\":";
  j += String(r.duration_ms);
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"hid_interrupt_lab_loop\"";
  j += ",\"note\":\"class_callback_not_l0_wire_not_hu\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleHidInject() {
  if (!server_) {
    return;
  }
  if (!device_ || role_ != Role::Device) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"device_role_required\"}"));
    return;
  }
  uint8_t key = static_cast<uint8_t>('a');
  bool raw = false;
  if (server_->hasArg("ascii")) {
    const String a = server_->arg("ascii");
    if (a.length() > 0) {
      key = static_cast<uint8_t>(a.charAt(0));
    }
  }
  if (server_->hasArg("key")) {
    key = static_cast<uint8_t>(strtoul(server_->arg("key").c_str(), nullptr, 0));
  }
  if (server_->hasArg("raw")) {
    const String v = server_->arg("raw");
    raw = (v == "1" || v == "true" || v == "on");
  }
  const bool ok = device_->injectHidKey(key, raw);
  String j = "{\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"key\":";
  j += String(key);
  j += ",\"raw\":";
  j += raw ? "true" : "false";
  j += ",\"injects\":";
  j += String(device_->hidInjectCount());
  if (!ok) {
    j += ",\"err\":\"";
    j += device_->hidEnabled() ? "inject_fail_or_unmounted" : "hid_disabled";
    j += "\"";
  }
  j += ",\"claim\":\"hid_interrupt_lab_loop\"";
  j += ",\"note\":\"gadget_inject_not_hu\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleSoftReenum() {
  if (!server_) {
    return;
  }
  if (!device_ || role_ != Role::Device) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"device_role_required\"}"));
    return;
  }
  device_->requestSoftReenum();
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(
      200, F("application/json"),
      F("{\"ok\":true,\"scheduled\":true,\"claim\":\"usb_enum_outcome_lab\","
        "\"note\":\"soft_reenum_not_control_mitm\"}"));
}

void AnalyzerStatusModule::handleUsbSuspendResume() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  uint32_t hold_ms = 100;
  if (server_->hasArg("hold_ms")) {
    hold_ms = static_cast<uint32_t>(server_->arg("hold_ms").toInt());
  }
  String mode = "cycle";
  if (server_->hasArg("mode")) {
    mode = server_->arg("mode");
  }
  UsbHostNode::SuspendResumeResult r;
  if (mode == "suspend") {
    r = host_->runBusSuspend(hold_ms);
  } else if (mode == "resume") {
    r = host_->runBusResume();
  } else {
    r = host_->runBusSuspendResume(hold_ms);
  }
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"suspended\":";
  j += r.suspended ? "true" : "false";
  j += ",\"resumed\":";
  j += r.resumed ? "true" : "false";
  j += ",\"postTrafficOk\":";
  j += r.post_traffic_ok ? "true" : "false";
  j += ",\"hold_ms\":";
  j += String(r.hold_ms);
  j += ",\"duration_ms\":";
  j += String(r.duration_ms);
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"usb_bus_suspend_resume_lab\"";
  j += ",\"note\":\"hprt_stack_suspend_not_l0_phy\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleL0Feasibility() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  const UsbHostNode::L0FeasibilityResult r = host_->runL0Feasibility();
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"hostBegun\":";
  j += r.host_begun ? "true" : "false";
  j += ",\"dutPresent\":";
  j += r.dut_present ? "true" : "false";
  j += ",\"hprtRaw\":";
  j += String(r.hprt_raw);
  j += ",\"prtConn\":";
  j += r.prt_conn ? "true" : "false";
  j += ",\"prtEna\":";
  j += r.prt_ena ? "true" : "false";
  j += ",\"prtSusp\":";
  j += r.prt_susp ? "true" : "false";
  j += ",\"prtOvercur\":";
  j += r.prt_overcur ? "true" : "false";
  j += ",\"prtSpeed\":";
  j += String(r.prt_speed);
  j += ",\"gpio19_Dm\":";
  j += String(r.gpio19_level);
  j += ",\"gpio20_Dp\":";
  j += String(r.gpio20_level);
  j += ",\"pathA_snifferOk\":";
  j += r.path_a_sniffer_ok ? "true" : "false";
  j += ",\"pathA_reason\":\"";
  j += r.path_a_reason;
  j += "\",\"pathB_preferred\":true";
  j += ",\"pinsFact\":\"GPIO20=D+_GPIO19=D-_internal_PHY\"";
  if (r.note.length()) {
    j += ",\"note\":\"";
    j += r.note;
    j += "\"";
  }
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"gate10a_feasibility\"";
  j += ",\"gate\":\"10A\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleE11GpioSample() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  uint32_t samples = 200000;
  uint32_t traffic_blocks = 32;
  if (server_->hasArg("samples")) {
    samples = static_cast<uint32_t>(server_->arg("samples").toInt());
  }
  if (server_->hasArg("traffic_blocks")) {
    traffic_blocks =
        static_cast<uint32_t>(server_->arg("traffic_blocks").toInt());
  }
  const UsbHostNode::E11GpioSampleResult r =
      host_->runE11GpioSample(samples, traffic_blocks);
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"hostBegun\":";
  j += r.host_begun ? "true" : "false";
  j += ",\"dutBefore\":";
  j += r.dut_before ? "true" : "false";
  j += ",\"dutAfter\":";
  j += r.dut_after ? "true" : "false";
  j += ",\"prtConnBefore\":";
  j += r.prt_conn_before ? "true" : "false";
  j += ",\"prtConnAfter\":";
  j += r.prt_conn_after ? "true" : "false";
  j += ",\"prtEnaBefore\":";
  j += r.prt_ena_before ? "true" : "false";
  j += ",\"prtEnaAfter\":";
  j += r.prt_ena_after ? "true" : "false";
  j += ",\"hprtBefore\":";
  j += String(r.hprt_before);
  j += ",\"hprtAfter\":";
  j += String(r.hprt_after);
  j += ",\"samples\":";
  j += String(r.samples);
  j += ",\"durationUs\":";
  j += String(r.duration_us);
  j += ",\"rateKsps\":";
  j += String(r.rate_ksps);
  j += ",\"transitionsDm\":";
  j += String(r.transitions_dm);
  j += ",\"transitionsDp\":";
  j += String(r.transitions_dp);
  j += ",\"sampleDtMinCy\":";
  j += String(r.sample_dt_min_cy);
  j += ",\"sampleDtMaxCy\":";
  j += String(r.sample_dt_max_cy);
  j += ",\"sampleDtAvgCy\":";
  j += String(r.sample_dt_avg_cy);
  j += ",\"cpuMhz\":";
  j += String(r.cpu_mhz);
  j += ",\"jitterPpNs\":";
  j += String(r.jitter_pp_ns);
  j += ",\"trafficOk\":";
  j += r.traffic_ok ? "true" : "false";
  j += ",\"trafficRateKBs\":";
  j += String(r.traffic_rate_kBs);
  j += ",\"postProbeOk\":";
  j += r.post_probe_ok ? "true" : "false";
  j += ",\"busStable\":";
  j += r.bus_stable ? "true" : "false";
  j += ",\"destructive\":";
  j += r.destructive ? "true" : "false";
  j += ",\"meaningfulEdges\":";
  j += r.meaningful_edges ? "true" : "false";
  j += ",\"pathA_usable\":";
  j += r.path_a_usable ? "true" : "false";
  j += ",\"pathA_reason\":\"";
  j += r.path_a_reason;
  j += "\",\"pathB_preferred\":true";
  j += ",\"pinsFact\":\"GPIO20=D+_GPIO19=D-_internal_PHY\"";
  j += ",\"mode\":\"gpio_peek_poll_no_remux\"";
  if (r.note.length()) {
    j += ",\"note\":\"";
    j += r.note;
    j += "\"";
  }
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"durationMs\":";
  j += String(r.duration_ms);
  j += ",\"claim\":\"gate10a_e11_gpio_sample\"";
  j += ",\"experiment\":\"E1.1\"";
  j += ",\"gate\":\"10A\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handlePathBGpio() {
  if (!server_) {
    return;
  }
  String mode = server_->hasArg("mode") ? server_->arg("mode") : "txrx";
  String pattern =
      server_->hasArg("pattern") ? server_->arg("pattern") : "setup_ok";
  uint32_t rate = 100000;
  uint32_t jitter = 0;
  int dp = PathBGpioLab::kDefaultDpPin;
  int dm = PathBGpioLab::kDefaultDmPin;
  int sync = PathBGpioLab::kDefaultSyncPin;
  uint16_t l0st_port = PathBGpioLab::kDefaultL0stPort;
  String l0st_host = controller_host_;
  if (server_->hasArg("rate")) {
    rate = static_cast<uint32_t>(server_->arg("rate").toInt());
  }
  if (server_->hasArg("jitter")) {
    jitter = static_cast<uint32_t>(server_->arg("jitter").toInt());
  }
  if (server_->hasArg("dp")) {
    dp = server_->arg("dp").toInt();
  }
  if (server_->hasArg("dm")) {
    dm = server_->arg("dm").toInt();
  }
  if (server_->hasArg("sync")) {
    sync = server_->arg("sync").toInt();
  }
  if (server_->hasArg("l0st_host")) {
    l0st_host = server_->arg("l0st_host");
  }
  if (server_->hasArg("l0st_port")) {
    l0st_port = static_cast<uint16_t>(server_->arg("l0st_port").toInt());
  }
  PathBGpioLab lab;
  const PathBGpioLab::Result r = lab.run(mode, rate, dp, dm, l0st_host,
                                         l0st_port, pattern, jitter, sync, true);
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"mode\":\"";
  j += r.mode;
  j += "\",\"pattern\":\"";
  j += r.pattern;
  j += "\",\"dpPin\":";
  j += String(r.dp_pin);
  j += ",\"dmPin\":";
  j += String(r.dm_pin);
  j += ",\"syncPin\":";
  j += String(r.sync_pin);
  j += ",\"rateHz\":";
  j += String(r.rate_hz);
  j += ",\"jitterPct\":";
  j += String(r.jitter_pct);
  j += ",\"achievedRateHz\":";
  j += String(r.achieved_rate_hz);
  j += ",\"statesTx\":";
  j += String(r.states_tx);
  j += ",\"statesRx\":";
  j += String(r.states_rx);
  j += ",\"mismatches\":";
  j += String(r.mismatches);
  j += ",\"loopbackOk\":";
  j += r.loopback_ok ? "true" : "false";
  j += ",\"syncPulsed\":";
  j += r.sync_pulsed ? "true" : "false";
  j += ",\"l0stOk\":";
  j += r.l0st_ok ? "true" : "false";
  j += ",\"l0stBytes\":";
  j += String(r.l0st_bytes);
  j += ",\"cyclesPerUs\":";
  j += String(GpioBitbangTiming::cyclesPerUs());
  j += ",\"durationUs\":";
  j += String(r.duration_us);
  j += ",\"note\":\"";
  j += r.note;
  j += "\"";
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"path_b_gpio_lab\"";
  j += ",\"notClaimed\":\"live_otg_dplus_dminus_phy\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handlePathBGpioRx() {
  if (!server_) {
    return;
  }
  String mode = server_->hasArg("mode") ? server_->arg("mode") : "rx";
  uint32_t samples = 256;
  uint32_t rate = 100000;
  uint32_t timeout_ms = 3000;
  int dp = PathBGpioRx::kDefaultDpPin;
  int dm = PathBGpioRx::kDefaultDmPin;
  int sync = PathBGpioRx::kDefaultSyncPin;
  uint16_t l0st_port = PathBGpioRx::kDefaultL0stPort;
  String l0st_host = controller_host_;
  if (server_->hasArg("samples")) {
    samples = static_cast<uint32_t>(server_->arg("samples").toInt());
  }
  if (server_->hasArg("rate")) {
    rate = static_cast<uint32_t>(server_->arg("rate").toInt());
  }
  if (server_->hasArg("timeout_ms")) {
    timeout_ms = static_cast<uint32_t>(server_->arg("timeout_ms").toInt());
  }
  if (server_->hasArg("dp")) {
    dp = server_->arg("dp").toInt();
  }
  if (server_->hasArg("dm")) {
    dm = server_->arg("dm").toInt();
  }
  if (server_->hasArg("sync")) {
    sync = server_->arg("sync").toInt();
  }
  if (server_->hasArg("l0st_host")) {
    l0st_host = server_->arg("l0st_host");
  }
  if (server_->hasArg("l0st_port")) {
    l0st_port = static_cast<uint16_t>(server_->arg("l0st_port").toInt());
  }
  const PathBGpioRx::Result r =
      PathBGpioRx::run(mode, samples, rate, dp, dm, sync, timeout_ms, l0st_host,
                       l0st_port);
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"mode\":\"";
  j += r.mode;
  j += "\",\"samples\":";
  j += String(r.samples);
  j += ",\"edges\":";
  j += String(r.edges);
  j += ",\"histSe0\":";
  j += String(r.hist[0]);
  j += ",\"histJ\":";
  j += String(r.hist[1]);
  j += ",\"histK\":";
  j += String(r.hist[2]);
  j += ",\"histSe1\":";
  j += String(r.hist[3]);
  j += ",\"syncSeen\":";
  j += r.sync_seen ? "true" : "false";
  j += ",\"syncTimeout\":";
  j += r.sync_timeout ? "true" : "false";
  j += ",\"achievedRateHz\":";
  j += String(r.achieved_rate_hz);
  j += ",\"l0stOk\":";
  j += r.l0st_ok ? "true" : "false";
  j += ",\"durationUs\":";
  j += String(r.duration_us);
  j += ",\"jumperLikely\":";
  j += (r.edges > 4) ? "true" : "false";
  j += ",\"note\":\"";
  j += r.note;
  j += "\"";
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"path_b_gpio_rx_lab\"";
  j += ",\"notClaimed\":\"otg_phy_sniffer\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleGpioCalibrate() {
  if (!server_) {
    return;
  }
  uint32_t spin = 1000;
  if (server_->hasArg("spin_us")) {
    spin = static_cast<uint32_t>(server_->arg("spin_us").toInt());
  }
  const GpioBitbangTiming::CalResult r = GpioBitbangTiming::calibrate(spin);
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"cpuMhz\":";
  j += String(r.cpu_mhz);
  j += ",\"cyclesPerUs\":";
  j += String(r.cycles_per_us);
  j += ",\"measureUs\":";
  j += String(r.measure_us);
  j += ",\"measuredCycles\":";
  j += String(r.measured_cycles);
  j += ",\"errPpm\":";
  j += String(r.err_ppm);
  j += ",\"note\":\"";
  j += r.note;
  j += "\",\"claim\":\"gpio_bitbang_calibrate_lab\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
}

void AnalyzerStatusModule::handleWireStimulus() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  const String confirm =
      server_->hasArg("confirm") ? server_->arg("confirm") : "";
  if (confirm != "wire_stimulus_destructive") {
    server_->send(
        400, F("application/json"),
        F("{\"ok\":false,\"err\":\"confirm_required\","
          "\"need\":\"confirm=wire_stimulus_destructive\","
          "\"warn\":\"stops_usb_host_claims_gpio19_20_reboot_to_restore\"}"));
    return;
  }
  String pattern =
      server_->hasArg("pattern") ? server_->arg("pattern") : "bus_states";
  String patterns =
      server_->hasArg("patterns") ? server_->arg("patterns") : "";
  uint32_t rate = 100000;
  uint32_t reboot_ms = 2500;
  uint32_t gap_ms = 20;
  if (server_->hasArg("rate")) {
    rate = static_cast<uint32_t>(server_->arg("rate").toInt());
  }
  if (server_->hasArg("reboot_ms")) {
    reboot_ms = static_cast<uint32_t>(server_->arg("reboot_ms").toInt());
  }
  if (server_->hasArg("gap_ms")) {
    gap_ms = static_cast<uint32_t>(server_->arg("gap_ms").toInt());
  }
  String rel_err;
  WireStimulus::Result r;
  if (!host_->releaseHostStack(rel_err)) {
    r.error = rel_err.length() ? rel_err : "release_failed";
  } else {
    CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo, 0, rate, 0, 0,
                                0, "wire_stimulus_begin");
    if (patterns.length()) {
      r = WireStimulus::runSeries(patterns, rate, reboot_ms, gap_ms);
    } else {
      r = WireStimulus::run(pattern, rate, reboot_ms, gap_ms);
    }
    CaptureLog::instance().push(CapRole::Host, CapType::EnumInfo,
                                r.ok ? 0 : 1, r.states, r.achieved_rate_hz, 0,
                                0, "wire_stimulus_end");
  }
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"pattern\":\"";
  j += r.pattern;
  j += "\",\"rateHz\":";
  j += String(r.rate_hz);
  j += ",\"achievedRateHz\":";
  j += String(r.achieved_rate_hz);
  j += ",\"states\":";
  j += String(r.states);
  j += ",\"durationUs\":";
  j += String(r.duration_us);
  j += ",\"activeUs\":";
  j += String(r.active_us);
  j += ",\"hostReleased\":";
  j += r.host_released ? "true" : "false";
  j += ",\"pinsClaimed\":";
  j += r.pins_claimed ? "true" : "false";
  j += ",\"rebootAfterMs\":";
  j += String(r.reboot_after_ms);
  j += ",\"pins\":\"GPIO20=D+_GPIO19=D-_driven\"";
  j += ",\"note\":\"";
  j += r.note;
  j += "\"";
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"wire_stimulus_lab\"";
  j += ",\"destructive\":true";
  j += ",\"notClaimed\":\"usb_host_parallel_operation\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
  if (r.ok && reboot_ms > 0) {
    delay(50);
    Serial.printf("[host] wire-stimulus reboot in %u ms\n",
                  static_cast<unsigned>(reboot_ms));
    delay(reboot_ms);
    ESP.restart();
  }
}

void AnalyzerStatusModule::handleUsbCtrlNeg() {
  if (!server_) {
    return;
  }
  if (!host_ || role_ != Role::Host) {
    server_->send(400, F("application/json"),
                  F("{\"ok\":false,\"err\":\"host_role_required\"}"));
    return;
  }
  const UsbHostNode::CtrlNegResult r = host_->runCtrlNegSeries();
  String j = "{\"ok\":";
  j += r.ok ? "true" : "false";
  j += ",\"cases\":";
  j += String(r.cases);
  j += ",\"passed\":";
  j += String(r.passed);
  j += ",\"duration_ms\":";
  j += String(r.duration_ms);
  j += ",\"detail\":[";
  for (uint8_t i = 0; i < r.cases; i++) {
    if (i) {
      j += ",";
    }
    const auto &c = r.detail[i];
    j += "{\"name\":\"";
    j += c.name;
    j += "\",\"ok\":";
    j += c.ok ? "true" : "false";
    j += ",\"expectOk\":";
    j += c.expect_ok ? "true" : "false";
    j += ",\"pass\":";
    j += c.pass ? "true" : "false";
    j += ",\"actual\":";
    j += String(c.actual);
    j += ",\"hostErr\":";
    j += String(c.host_err);
    j += "}";
  }
  j += "]";
  if (r.error.length()) {
    j += ",\"err\":\"";
    j += r.error;
    j += "\"";
  }
  j += ",\"claim\":\"usb_ctrl_neg_lab\"";
  j += ",\"note\":\"caller_ep0_negatives_not_setup_mitm\"}";
  server_->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server_->send(r.ok ? 200 : 500, F("application/json"), j);
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
