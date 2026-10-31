#pragma once

/**
 * Gate 3 — TCP integrity data plane (ESP↔ESP).
 * Gate 4 slice — windowed bulk + MSC READ proxy over same port.
 *
 * Port 9752. Framing: magic INTG | ver | cmd | seq | len | crc32 | payload
 * Not wire capture. Not claimed transparent MITM.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

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
  uint32_t rtt_min_us = 0;
  uint32_t rtt_avg_us = 0;
  uint32_t rtt_p50_us = 0;
  uint32_t rtt_p95_us = 0;
  uint32_t rtt_max_us = 0;
  String mode; // "ack_each" | "windowed"
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
   * Raises WiFi TCP throughput toward link capacity (Gate 4 substrate).
   */
  IntegrityResult runBulk(const String &peer_ip, uint32_t frames,
                          uint32_t frame_bytes, uint32_t window);

  /**
   * Device→Host MSC proxy: ask peer Host to READ blocks from its DUT.
   * @return true and fills out[blocks*512] on success.
   */
  bool proxyMscRead(const String &peer_ip, uint32_t lba, uint32_t blocks,
                    uint8_t *out, uint32_t out_len, String *err = nullptr);

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

private:
  WiFiServer server_{kPort};
  WiFiClient inbound_;
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
  static uint32_t crc32(const uint8_t *data, size_t len);
  static void fillPattern(uint8_t *buf, uint32_t len, uint32_t seq);
  static bool checkPattern(const uint8_t *buf, uint32_t len, uint32_t seq);
  bool readExact(WiFiClient &c, uint8_t *buf, size_t n, uint32_t timeout_ms);
  bool writeExact(WiFiClient &c, const uint8_t *buf, size_t n);
};

} // namespace usb_analyzer
