#include "protocol/DataPlane.h"

#include "capture/CaptureLog.h"
#include "host/UsbHostNode.h"

#include <algorithm>
#include <cstring>
#include <esp_timer.h>

namespace usb_analyzer {
namespace {

constexpr uint32_t kMagic = 0x494E5447u; // 'INTG'
constexpr uint16_t kVer = 1;
constexpr uint8_t kCmdPush = 1;
constexpr uint8_t kCmdAck = 2;
constexpr uint8_t kCmdMscReq = 20;
constexpr uint8_t kCmdMscRsp = 21;
constexpr uint8_t kCmdMscWriteReq = 22;
constexpr uint8_t kCmdMscWriteRsp = 23;
constexpr uint32_t kMaxFrame = 4096;
constexpr uint32_t kMaxMscBlocks = 8;

#pragma pack(push, 1)
struct Hdr {
  uint32_t magic;
  uint16_t ver;
  uint8_t cmd;
  uint8_t status;
  uint32_t seq;
  uint32_t len;
  uint32_t crc;
};
struct MscReq {
  uint32_t lba;
  uint16_t blocks;
  uint16_t reserved;
};
#pragma pack(pop)

static_assert(sizeof(Hdr) == 20, "Hdr size");
static_assert(sizeof(MscReq) == 8, "MscReq size");

} // namespace

uint32_t DataPlane::crc32(const uint8_t *data, size_t len) {
  static uint32_t table[256];
  static bool ready = false;
  if (!ready) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int b = 0; b < 8; b++) {
        c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      }
      table[i] = c;
    }
    ready = true;
  }
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
  }
  return ~crc;
}

void DataPlane::fillPattern(uint8_t *buf, uint32_t len, uint32_t seq) {
  for (uint32_t i = 0; i < len; i++) {
    buf[i] = static_cast<uint8_t>((seq + i) & 0xff);
  }
}

bool DataPlane::checkPattern(const uint8_t *buf, uint32_t len, uint32_t seq) {
  for (uint32_t i = 0; i < len; i++) {
    if (buf[i] != static_cast<uint8_t>((seq + i) & 0xff)) {
      return false;
    }
  }
  return true;
}

bool DataPlane::readExact(WiFiClient &c, uint8_t *buf, size_t n,
                          uint32_t timeout_ms) {
  size_t got = 0;
  const uint32_t start = millis();
  while (got < n) {
    if (!c.connected()) {
      return false;
    }
    const int avail = c.available();
    if (avail > 0) {
      const size_t chunk =
          c.read(buf + got, static_cast<size_t>(min<int>(avail, n - got)));
      got += chunk;
      continue;
    }
    if (millis() - start > timeout_ms) {
      return false;
    }
    delay(1);
  }
  return true;
}

bool DataPlane::writeExact(WiFiClient &c, const uint8_t *buf, size_t n) {
  size_t sent = 0;
  while (sent < n) {
    if (!c.connected()) {
      return false;
    }
    const size_t w = c.write(buf + sent, n - sent);
    if (w == 0) {
      delay(1);
      continue;
    }
    sent += w;
  }
  return true;
}

void DataPlane::begin() {
  if (WiFi.status() != WL_CONNECTED) {
    state_ = "no_wifi";
    return;
  }
  server_.begin();
  started_ = true;
  state_ = "listen";
  Serial.printf("[data] plane listen :%u (INTG + MSC proxy)\n",
                static_cast<unsigned>(kPort));
}

void DataPlane::acceptIfNeeded() {
  if (!started_) {
    if (WiFi.status() == WL_CONNECTED) {
      begin();
    }
    return;
  }
  if (!inbound_ || !inbound_.connected()) {
    WiFiClient c = server_.available();
    if (c) {
      inbound_ = c;
      inbound_.setNoDelay(true);
      has_expect_ = false;
      expect_seq_ = 0;
      state_ = "peer_in";
      Serial.printf("[data] inbound from %s\n",
                    inbound_.remoteIP().toString().c_str());
    }
  }
}

bool DataPlane::handlePush(WiFiClient &c, const uint8_t *hdr_bytes) {
  Hdr hdr;
  memcpy(&hdr, hdr_bytes, sizeof(hdr));
  if (hdr.len > kMaxFrame) {
    return false;
  }
  static uint8_t buf[kMaxFrame];
  if (hdr.len > 0 && !readExact(c, buf, hdr.len, 5000)) {
    return false;
  }
  rx_bytes_ += sizeof(Hdr) + hdr.len;
  uint8_t status = 0;
  const uint32_t crc = crc32(buf, hdr.len);
  if (crc != hdr.crc) {
    status = 1;
    gaps_++;
  } else if (!checkPattern(buf, hdr.len, hdr.seq)) {
    status = 2;
    gaps_++;
  } else if (has_expect_ && hdr.seq != expect_seq_) {
    status = 3;
    gaps_++;
  }
  if (status == 0) {
    expect_seq_ = hdr.seq + 1;
    has_expect_ = true;
  }
  Hdr ack{};
  ack.magic = kMagic;
  ack.ver = kVer;
  ack.cmd = kCmdAck;
  ack.status = status;
  ack.seq = hdr.seq;
  ack.len = 0;
  ack.crc = 0;
  if (!writeExact(c, reinterpret_cast<uint8_t *>(&ack), sizeof(ack))) {
    return false;
  }
  tx_bytes_ += sizeof(ack);
  return true;
}

bool DataPlane::handleMscReq(WiFiClient &c, const uint8_t *hdr_bytes) {
  Hdr hdr;
  memcpy(&hdr, hdr_bytes, sizeof(hdr));
  if (hdr.len != sizeof(MscReq)) {
    return false;
  }
  MscReq req{};
  if (!readExact(c, reinterpret_cast<uint8_t *>(&req), sizeof(req), 2000)) {
    return false;
  }
  rx_bytes_ += sizeof(Hdr) + sizeof(req);

  static uint8_t sector_buf[512 * kMaxMscBlocks];
  const uint32_t blocks = req.blocks;

  Hdr rsp{};
  rsp.magic = kMagic;
  rsp.ver = kVer;
  rsp.cmd = kCmdMscRsp;
  rsp.seq = hdr.seq;
  rsp.status = 0;
  rsp.len = 0;
  rsp.crc = 0;

  if (blocks == 0 || blocks > kMaxMscBlocks) {
    rsp.status = 2;
  } else if (!host_ || host_->deviceCount() == 0) {
    rsp.status = 3;
  } else if (!host_->readBlocks(req.lba, blocks, sector_buf)) {
    rsp.status = 4;
    proxy_fail_++;
  } else {
    rsp.len = blocks * 512u;
    rsp.crc = crc32(sector_buf, rsp.len);
    proxy_hits_++;
  }

  if (!writeExact(c, reinterpret_cast<uint8_t *>(&rsp), sizeof(rsp))) {
    return false;
  }
  tx_bytes_ += sizeof(rsp);
  if (rsp.status == 0 && rsp.len > 0) {
    if (!writeExact(c, sector_buf, rsp.len)) {
      return false;
    }
    tx_bytes_ += rsp.len;
  }
  return true;
}

void DataPlane::serviceInbound() {
  if (!inbound_ || !inbound_.connected()) {
    if (strcmp(state_, "peer_in") == 0) {
      state_ = "listen";
    }
    return;
  }
  if (static_cast<size_t>(inbound_.available()) < sizeof(Hdr)) {
    return;
  }
  uint8_t raw[sizeof(Hdr)];
  if (!readExact(inbound_, raw, sizeof(Hdr), 2000)) {
    inbound_.stop();
    gaps_++;
    return;
  }
  Hdr hdr;
  memcpy(&hdr, raw, sizeof(hdr));
  if (hdr.magic != kMagic || hdr.ver != kVer) {
    Serial.println("[data] bad magic — drop");
    inbound_.stop();
    gaps_++;
    return;
  }
  bool ok = false;
  if (hdr.cmd == kCmdPush) {
    ok = handlePush(inbound_, raw);
  } else if (hdr.cmd == kCmdMscReq) {
    ok = handleMscReq(inbound_, raw);
  } else if (hdr.cmd == kCmdMscWriteReq) {
    ok = handleMscWriteReq(inbound_, raw);
  } else {
    Serial.printf("[data] unknown cmd %u\n", hdr.cmd);
  }
  if (!ok) {
    inbound_.stop();
    gaps_++;
  }
}

void DataPlane::loop() {
  acceptIfNeeded();
  serviceInbound();
}

IntegrityResult DataPlane::runIntegrity(const String &peer_ip, uint32_t frames,
                                        uint32_t frame_bytes) {
  return runBulk(peer_ip, frames, frame_bytes, 1);
}

IntegrityResult DataPlane::runBulk(const String &peer_ip, uint32_t frames,
                                   uint32_t frame_bytes, uint32_t window) {
  IntegrityResult r;
  last_peer_ = peer_ip;
  r.mode = (window <= 1) ? "ack_each" : "windowed";
  r.window = window < 1 ? 1 : window;
  if (window < 1) {
    window = 1;
  }
  if (window > 32) {
    window = 32;
  }
  r.window = window;

  if (frame_bytes == 0 || frame_bytes > kMaxFrame) {
    r.error = "bad_frame_bytes";
    last_result_ = r;
    has_last_ = true;
    last_match_ = false;
    return r;
  }
  if (frames == 0 || frames > 4096) {
    r.error = "bad_frames";
    last_result_ = r;
    has_last_ = true;
    last_match_ = false;
    return r;
  }
  if (WiFi.status() != WL_CONNECTED) {
    r.error = "no_wifi";
    last_result_ = r;
    has_last_ = true;
    last_match_ = false;
    return r;
  }

  WiFiClient client;
  client.setNoDelay(true);
  state_ = "connecting";
  if (!client.connect(peer_ip.c_str(), kPort, 3000)) {
    r.error = "connect_fail";
    state_ = started_ ? "listen" : "offline";
    last_result_ = r;
    has_last_ = true;
    last_match_ = false;
    return r;
  }
  state_ = "push";
  snprintf(last_pattern_, sizeof(last_pattern_), "bulk win=%u frames=%u size=%u",
           static_cast<unsigned>(window), static_cast<unsigned>(frames),
           static_cast<unsigned>(frame_bytes));

  static uint8_t buf[kMaxFrame];
  static uint32_t rtts[512];
  uint32_t rtt_n = 0;
  const uint32_t t0 = millis();

  for (uint32_t base = 0; base < frames;) {
    acceptIfNeeded();
    serviceInbound();

    const uint32_t batch =
        (base + window > frames) ? (frames - base) : window;
    const int64_t t_batch = esp_timer_get_time();

    for (uint32_t i = 0; i < batch; i++) {
      const uint32_t seq = base + i;
      fillPattern(buf, frame_bytes, seq);
      Hdr hdr{};
      hdr.magic = kMagic;
      hdr.ver = kVer;
      hdr.cmd = kCmdPush;
      hdr.status = 0;
      hdr.seq = seq;
      hdr.len = frame_bytes;
      hdr.crc = crc32(buf, frame_bytes);
      if (!writeExact(client, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)) ||
          !writeExact(client, buf, frame_bytes)) {
        r.error = "write_fail";
        client.stop();
        state_ = "listen";
        last_result_ = r;
        has_last_ = true;
        last_match_ = false;
        return r;
      }
      r.frames_sent++;
      r.bytes_tx += sizeof(hdr) + frame_bytes;
      tx_bytes_ += sizeof(hdr) + frame_bytes;
    }

    for (uint32_t i = 0; i < batch; i++) {
      Hdr ack{};
      if (!readExact(client, reinterpret_cast<uint8_t *>(&ack), sizeof(ack),
                     8000)) {
        r.error = "ack_timeout";
        client.stop();
        state_ = "listen";
        last_result_ = r;
        has_last_ = true;
        last_match_ = false;
        return r;
      }
      r.bytes_rx += sizeof(ack);
      rx_bytes_ += sizeof(ack);
      const uint32_t seq = base + i;
      if (ack.magic != kMagic || ack.cmd != kCmdAck || ack.seq != seq ||
          ack.status != 0) {
        r.frames_fail++;
        gaps_++;
        r.error = "ack_fail_status_" + String(ack.status);
        client.stop();
        state_ = "listen";
        last_match_ = false;
        has_last_ = true;
        last_result_ = r;
        return r;
      }
      r.frames_acked++;
    }
    if (rtt_n < 512u) {
      rtts[rtt_n++] =
          static_cast<uint32_t>(esp_timer_get_time() - t_batch) / batch;
    }
    base += batch;
  }

  client.stop();
  r.duration_ms = millis() - t0;
  r.ok = (r.frames_acked == frames && r.frames_fail == 0);
  last_match_ = r.ok;
  has_last_ = true;
  if (r.duration_ms > 0) {
    rate_kbs_ = static_cast<uint32_t>(
        (static_cast<uint64_t>(r.bytes_tx) * 1000ull / r.duration_ms) / 1024ull);
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
  state_ = "listen";
  last_result_ = r;
  CaptureLog::instance().push(
      CapRole::Plane, CapType::TcpBulk, r.ok ? 0 : 1, r.bytes_tx, frames, window,
      r.duration_ms * 1000u, r.mode.c_str());
  Serial.printf(
      "[data] bulk %s → %s win=%u frames=%u/%u %u ms %u kB/s\n",
      r.ok ? "OK" : "FAIL", peer_ip.c_str(), static_cast<unsigned>(window),
      static_cast<unsigned>(r.frames_acked), static_cast<unsigned>(frames),
      static_cast<unsigned>(r.duration_ms), static_cast<unsigned>(rate_kbs_));
  return r;
}

bool DataPlane::handleMscWriteReq(WiFiClient &c, const uint8_t *hdr_bytes) {
  Hdr hdr;
  memcpy(&hdr, hdr_bytes, sizeof(hdr));
  if (hdr.len != sizeof(MscReq)) {
    return false;
  }
  MscReq req{};
  if (!readExact(c, reinterpret_cast<uint8_t *>(&req), sizeof(req), 2000)) {
    return false;
  }
  rx_bytes_ += sizeof(Hdr) + sizeof(req);

  static uint8_t sector_buf[512 * kMaxMscBlocks];
  const uint32_t blocks = req.blocks;

  Hdr rsp{};
  rsp.magic = kMagic;
  rsp.ver = kVer;
  rsp.cmd = kCmdMscWriteRsp;
  rsp.seq = hdr.seq;
  rsp.status = 0;
  rsp.len = 0;
  rsp.crc = 0;

  if (blocks == 0 || blocks > kMaxMscBlocks) {
    rsp.status = 2;
  } else if (!host_ || host_->deviceCount() == 0) {
    rsp.status = 3;
  } else {
    const uint32_t expect = blocks * 512u;
    if (!readExact(c, sector_buf, expect, 8000)) {
      return false;
    }
    rx_bytes_ += expect;
    if (!host_->writeBlocks(req.lba, blocks, sector_buf)) {
      rsp.status = 4;
      proxy_fail_++;
    } else {
      proxy_hits_++;
      CaptureLog::instance().push(CapRole::Plane, CapType::MscProxyRsp, 0, expect,
                                  req.lba, blocks, 0, "write_ok");
    }
  }

  if (!writeExact(c, reinterpret_cast<uint8_t *>(&rsp), sizeof(rsp))) {
    return false;
  }
  tx_bytes_ += sizeof(rsp);
  return true;
}

bool DataPlane::proxyMscRead(const String &peer_ip, uint32_t lba,
                             uint32_t blocks, uint8_t *out, uint32_t out_len,
                             String *err) {
  auto fail = [&](const char *e) {
    if (err) {
      *err = e;
    }
    proxy_fail_++;
    return false;
  };
  if (!out || blocks == 0 || blocks > kMaxMscBlocks) {
    return fail("bad_args");
  }
  if (out_len < blocks * 512u) {
    return fail("buf_small");
  }
  if (WiFi.status() != WL_CONNECTED) {
    return fail("no_wifi");
  }

  WiFiClient client;
  client.setNoDelay(true);
  if (!client.connect(peer_ip.c_str(), kPort, 2000)) {
    return fail("connect_fail");
  }

  MscReq req{};
  req.lba = lba;
  req.blocks = static_cast<uint16_t>(blocks);
  Hdr hdr{};
  hdr.magic = kMagic;
  hdr.ver = kVer;
  hdr.cmd = kCmdMscReq;
  hdr.status = 0;
  hdr.seq = lba;
  hdr.len = sizeof(req);
  hdr.crc = crc32(reinterpret_cast<uint8_t *>(&req), sizeof(req));

  if (!writeExact(client, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)) ||
      !writeExact(client, reinterpret_cast<uint8_t *>(&req), sizeof(req))) {
    client.stop();
    return fail("write_fail");
  }
  tx_bytes_ += sizeof(hdr) + sizeof(req);

  Hdr rsp{};
  if (!readExact(client, reinterpret_cast<uint8_t *>(&rsp), sizeof(rsp),
                 5000)) {
    client.stop();
    return fail("rsp_timeout");
  }
  if (rsp.magic != kMagic || rsp.cmd != kCmdMscRsp) {
    client.stop();
    return fail("bad_rsp");
  }
  CaptureLog::instance().push(CapRole::Plane, CapType::MscProxyReq, 0,
                              blocks * 512u, lba, blocks, 0, "to_host");
  if (rsp.status != 0 || rsp.len != blocks * 512u) {
    client.stop();
    if (err) {
      *err = "rsp_status_" + String(rsp.status);
    }
    proxy_fail_++;
    CaptureLog::instance().push(CapRole::Plane, CapType::MscProxyRsp, 1, 0, lba,
                                blocks, 0, "fail");
    return false;
  }
  if (!readExact(client, out, rsp.len, 8000)) {
    client.stop();
    return fail("payload_timeout");
  }
  rx_bytes_ += sizeof(rsp) + rsp.len;
  client.stop();
  proxy_hits_++;
  CaptureLog::instance().push(CapRole::Plane, CapType::MscProxyRsp, 0, rsp.len,
                              lba, blocks, 0, "ok");
  return true;
}

bool DataPlane::proxyMscWrite(const String &peer_ip, uint32_t lba,
                              uint32_t blocks, const uint8_t *in, uint32_t in_len,
                              String *err) {
  auto fail = [&](const char *e) {
    if (err) {
      *err = e;
    }
    proxy_fail_++;
    return false;
  };
  if (!in || blocks == 0 || blocks > kMaxMscBlocks) {
    return fail("bad_args");
  }
  if (in_len < blocks * 512u) {
    return fail("buf_small");
  }
  if (WiFi.status() != WL_CONNECTED) {
    return fail("no_wifi");
  }

  WiFiClient client;
  client.setNoDelay(true);
  if (!client.connect(peer_ip.c_str(), kPort, 2000)) {
    return fail("connect_fail");
  }

  MscReq req{};
  req.lba = lba;
  req.blocks = static_cast<uint16_t>(blocks);
  Hdr hdr{};
  hdr.magic = kMagic;
  hdr.ver = kVer;
  hdr.cmd = kCmdMscWriteReq;
  hdr.status = 0;
  hdr.seq = lba;
  hdr.len = sizeof(req);
  hdr.crc = crc32(reinterpret_cast<const uint8_t *>(&req), sizeof(req));

  const uint32_t payload = blocks * 512u;
  if (!writeExact(client, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)) ||
      !writeExact(client, reinterpret_cast<uint8_t *>(&req), sizeof(req)) ||
      !writeExact(client, in, payload)) {
    client.stop();
    return fail("write_fail");
  }
  tx_bytes_ += sizeof(hdr) + sizeof(req) + payload;
  CaptureLog::instance().push(CapRole::Plane, CapType::MscProxyReq, 0, payload,
                              lba, blocks, 0, "write_to_host");

  Hdr rsp{};
  if (!readExact(client, reinterpret_cast<uint8_t *>(&rsp), sizeof(rsp),
                 8000)) {
    client.stop();
    return fail("rsp_timeout");
  }
  client.stop();
  rx_bytes_ += sizeof(rsp);
  if (rsp.magic != kMagic || rsp.cmd != kCmdMscWriteRsp || rsp.status != 0) {
    if (err) {
      *err = "rsp_status_" + String(rsp.status);
    }
    proxy_fail_++;
    CaptureLog::instance().push(CapRole::Plane, CapType::MscProxyRsp, 1, 0, lba,
                                blocks, 0, "write_fail");
    return false;
  }
  proxy_hits_++;
  CaptureLog::instance().push(CapRole::Plane, CapType::MscProxyRsp, 0, payload,
                              lba, blocks, 0, "write_ack");
  return true;
}

} // namespace usb_analyzer
