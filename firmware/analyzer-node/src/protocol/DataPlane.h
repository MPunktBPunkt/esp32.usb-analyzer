#pragma once

/**
 * Gate 3 — TCP integrity data plane (ESP↔ESP).
 * Gate 4 — windowed bulk + MSC READ/WRITE proxy + DUT snapshot forward.
 *
 * Port 9752. Framing: magic INTG | ver | cmd | seq | len | crc32 | payload
 * Not wire capture. Not claimed transparent MITM / USB control MITM.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <usb_analyzer/DutInfoProxy.h>

namespace usb_analyzer {

class UsbHostNode;

struct IntegrityResult {
  bool ok = false;
  uint32_t frames_sent = 0;
  uint32_t frames_acked = 0;
  uint32_t frames_fail = 0;
  uint32_t bytes_tx = 0;
  uint32_t bytes_rx = 0;
  uint32_t duration_ms = 0;
  uint32_t window = 1;
  bool pipeline = false;
  uint32_t rtt_min_us = 0;
  uint32_t rtt_avg_us = 0;
  uint32_t rtt_p50_us = 0;
  uint32_t rtt_p95_us = 0;
  uint32_t rtt_max_us = 0;
  String mode; // "ack_each" | "windowed" | "pipeline"
  String error;
};

class DataPlane {
public:
  static constexpr uint16_t kPort = 9752;

  void begin();
  void loop();
  void setHostNode(UsbHostNode *host) { host_ = host; }

  /** Classic PUSH→wait ACK (latency / integrity). */
  IntegrityResult runIntegrity(const String &peer_ip, uint32_t frames,
                               uint32_t frame_bytes);

  /**
   * Windowed bulk: send `window` frames, then collect that many ACKs.
   * With pipeline=true: sliding window (ACK frees next send) — overlaps RTT.
   * Raises WiFi TCP throughput toward link capacity (Gate 4 substrate).
   */
  IntegrityResult runBulk(const String &peer_ip, uint32_t frames,
                          uint32_t frame_bytes, uint32_t window,
                          bool pipeline = false);

  /**
   * Device→Host MSC proxy: ask peer Host to READ blocks from its DUT.
   * Uses a persistent TCP session when possible.
   */
  bool proxyMscRead(const String &peer_ip, uint32_t lba, uint32_t blocks,
                    uint8_t *out, uint32_t out_len, String *err = nullptr);

  /** Device→Host MSC proxy WRITE: push blocks onto peer Host DUT. */
  bool proxyMscWrite(const String &peer_ip, uint32_t lba, uint32_t blocks,
                     const uint8_t *in, uint32_t in_len, String *err = nullptr);

  /** Device→Host: fetch DUT descriptor/inquiry snapshot (enum-forward slice). */
  bool proxyDutInfo(const String &peer_ip, DutInfoSnapshot &out,
                    String *err = nullptr);

  /**
   * Device→Host EP0 control IN/OUT (lab GET_DESCRIPTOR slice).
   * Not transparent MITM — caller-initiated over WiFi.
   */
  bool proxyControlTransfer(const String &peer_ip, uint8_t bm_request_type,
                            uint8_t b_request, uint16_t w_value,
                            uint16_t w_index, uint8_t *data, uint16_t w_length,
                            uint16_t *actual_len, String *err = nullptr);

  void closeProxySession();

  uint32_t txBytes() const { return tx_bytes_; }
  uint32_t rxBytes() const { return rx_bytes_; }
  uint32_t gaps() const { return gaps_; }
  uint32_t rateKBs() const { return rate_kbs_; }
  uint32_t proxyHits() const { return proxy_hits_; }
  uint32_t proxyFail() const { return proxy_fail_; }
  const char *state() const { return state_; }
  const char *lastPattern() const { return last_pattern_; }
  bool lastMatch() const { return last_match_; }
  bool hasLastResult() const { return has_last_; }
  const IntegrityResult &lastResult() const { return last_result_; }
  const String &lastPeer() const { return last_peer_; }

  static uint32_t crc32(const uint8_t *data, size_t len);

private:
  WiFiServer server_{kPort};
  WiFiClient inbound_;
  WiFiClient proxy_client_;
  String proxy_peer_ip_;
  UsbHostNode *host_ = nullptr;
  bool started_ = false;
  uint32_t tx_bytes_ = 0;
  uint32_t rx_bytes_ = 0;
  uint32_t gaps_ = 0;
  uint32_t rate_kbs_ = 0;
  uint32_t expect_seq_ = 0;
  bool has_expect_ = false;
  uint32_t proxy_hits_ = 0;
  uint32_t proxy_fail_ = 0;
  const char *state_ = "offline";
  char last_pattern_[48] = {0};
  bool last_match_ = false;
  bool has_last_ = false;
  IntegrityResult last_result_{};
  String last_peer_;

  void acceptIfNeeded();
  void serviceInbound();
  bool handlePush(WiFiClient &c, const uint8_t *hdr_bytes);
  bool handleMscReq(WiFiClient &c, const uint8_t *hdr_bytes);
  bool handleMscWriteReq(WiFiClient &c, const uint8_t *hdr_bytes);
  bool handleDutInfoReq(WiFiClient &c, const uint8_t *hdr_bytes);
  bool handleCtrlReq(WiFiClient &c, const uint8_t *hdr_bytes);
  bool ensureProxyClient(const String &peer_ip, String *err);
  static void fillPattern(uint8_t *buf, uint32_t len, uint32_t seq);
  static bool checkPattern(const uint8_t *buf, uint32_t len, uint32_t seq);
  bool readExact(WiFiClient &c, uint8_t *buf, size_t n, uint32_t timeout_ms);
  bool writeExact(WiFiClient &c, const uint8_t *buf, size_t n);
};

} // namespace usb_analyzer
