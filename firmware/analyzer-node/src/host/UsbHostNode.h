#pragma once

#include <Arduino.h>
#include <usb_analyzer/DutInfoProxy.h>

namespace usb_analyzer {

struct HostMscProbeResult {
  bool ok = false;
  bool msc_ready = false;
  bool pattern_ok = false;
  uint32_t blocks_read = 0;
  uint32_t block_size = 0;
  uint32_t chunk = 1;
  uint32_t bytes_read = 0;
  uint32_t duration_ms = 0;
  uint32_t rate_kBs = 0;
  uint32_t transfers = 0;
  uint32_t pattern_fail_bytes = 0;
  uint32_t rtt_min_us = 0;
  uint32_t rtt_avg_us = 0;
  uint32_t rtt_p50_us = 0;
  uint32_t rtt_p95_us = 0;
  uint32_t rtt_max_us = 0;
  uint64_t capacity_blocks = 0;
  String inquiry_vendor;
  String inquiry_product;
  String inquiry_revision;
  String dut_id; // "vid:pid"
  String error;
};

/**
 * Gate 1: EspUsbHost enumeration.
 * Gate 3.5 (lab): MSC probe Host→Device over OTG cable (not WiFi, not MITM).
 */
class UsbHostNode {
public:
  bool begin();
  void loop();
  bool ready() const { return ready_; }
  bool beginOk() const { return begin_ok_; }
  size_t deviceCount() const;
  String lastEventJson() const { return last_event_json_; }
  /** Host stack running; "connected" means ≥1 DUT on OTG. */
  const char *otgState() const;

  /** Append dut{} fields into status JSON (leading comma). */
  void appendStatusJson(String &json) const;

  /**
   * Timed MSC READ(10) against attached DUT (OTG loop / stick).
   * Verifies analyzer patterned gadget bytes when present.
   */
  /** @param chunk Blocks per MSC READ (1..8). Larger chunks raise throughput ceiling. */
  HostMscProbeResult runMscProbe(uint32_t blocks, uint32_t start_lba = 0,
                                 uint32_t chunk = 1);

  /**
   * Raw MSC READ/WRITE into caller buffer (Gate 4/6).
   * @param buf must hold blocks * 512 bytes; blocks 1..16.
   */
  bool readBlocks(uint32_t lba, uint32_t blocks, uint8_t *out);
  bool writeBlocks(uint32_t lba, uint32_t blocks, const uint8_t *in);

  /** WRITE then optional READ-back verify (OTG loop reverse direction). */
  HostMscProbeResult runMscWriteProbe(uint32_t blocks, uint32_t start_lba = 0,
                                      uint32_t chunk = 1, bool readback = true);

  bool hasLastProbe() const { return has_last_probe_; }
  const HostMscProbeResult &lastProbe() const { return last_probe_; }

  /** Fill Gate-4 DUT snapshot (descriptors + best-effort MSC inquiry). */
  bool fillDutSnapshot(DutInfoSnapshot &out);

  /**
   * Blocking EP0 control transfer against attached DUT (not from USB callback).
   * Gate-4 lab control-forward substrate.
   */
  bool controlTransfer(uint8_t bm_request_type, uint8_t b_request,
                       uint16_t w_value, uint16_t w_index, uint8_t *data,
                       uint16_t w_length, uint16_t *actual_len);

  /**
   * Lab AOA accessory handshake against DUT: 51 → 52×6 → 53.
   * Optional wait for re-enum to 18D1:2D00 (Device responder soft-reenum).
   * Claim: aoa_control_handshake_lab — NOT HU/BCL/accessory bulk.
   */
  struct AoaHandshakeResult {
    bool ok = false;
    uint16_t protocol = 0;
    bool protocol_ok = false;
    uint8_t strings_ok = 0;
    bool start_ok = false;
    bool reenum_ok = false;
    uint16_t dut_vid_before = 0;
    uint16_t dut_pid_before = 0;
    uint16_t dut_vid_after = 0;
    uint16_t dut_pid_after = 0;
    uint32_t duration_ms = 0;
    String profile;
    String error;
  };
  AoaHandshakeResult runAoaHandshake(const char *profile = "lab",
                                     bool wait_reenum = true);

  /** After accessory VID present: vendor bulk OUT + sync IN (echo). */
  struct AoaBulkResult {
    bool ok = false;
    bool opened = false;
    uint8_t ep_out = 0;
    uint8_t ep_in = 0;
    uint16_t pkt_out = 0;
    uint16_t pkt_in = 0;
    uint32_t tx = 0;
    uint32_t rx = 0;
    bool match = false;
    uint8_t rx_buf[64]{};
    uint8_t rx_len = 0;
    uint32_t duration_ms = 0;
    String error;
  };
  AoaBulkResult runAoaBulkEcho(const char *payload = "AOA-LAB");
  AoaBulkResult runAoaBulkEcho(const uint8_t *data, size_t len);

  /**
   * Lab BCL 8-byte BE frame over AOA vendor bulk (echo round-trip).
   * Parses **RX** bytes (not TX frame). Desk FACT bmw-bcl; NOT HU wire.
   */
  struct AoaBclResult {
    bool ok = false;
    bool init_ok = false;
    bool frame_ok = false;
    bool parse_ok = false;
    uint16_t cmd = 0;
    uint16_t src = 0;
    uint16_t dest = 0;
    uint16_t data_len = 0;
    uint32_t tx = 0;
    uint32_t rx = 0;
    uint32_t duration_ms = 0;
    String error;
  };
  /**
   * @param explicit_payload  false → synthetic 6-byte lab payload;
   *                          true → use payload/payload_len (len 0 = header-only).
   */
  AoaBclResult runAoaBclRoundtrip(uint16_t cmd = 4, uint16_t dest = 0x0FA4,
                                  bool send_init = true,
                                  const uint8_t *payload = nullptr,
                                  size_t payload_len = 0,
                                  bool explicit_payload = false);

  /**
   * Post-enum USB state outcome + caller EP0 GET_DESCRIPTOR/GET_CONFIGURATION.
   * Claim: usb_enum_outcome_lab — NOT mid-enum SETUP MITM / L0 / SET_ADDRESS wire.
   */
  struct EnumObserveResult {
    bool ok = false;
    bool dut_present = false;
    uint8_t address = 0;
    uint8_t configuration_value = 0;
    uint8_t iface_count = 0;
    bool hid_iface = false;
    bool get_desc_ok = false;
    bool get_config_ok = false;
    bool get_config_match = false;
    uint8_t get_config_byte = 0;
    uint32_t duration_ms = 0;
    String error;
  };
  EnumObserveResult runEnumObserve(bool ep0_probe = true);

  /**
   * Wait for HID interrupt report from DUT (EspUsbHost keyboard/HID callback).
   * Claim: hid_interrupt_lab_loop — NOT wire / MITM / HU.
   */
  struct HidObserveResult {
    bool ok = false;
    bool got = false;
    uint8_t keycode = 0;
    uint8_t ascii = 0;
    uint8_t modifiers = 0;
    uint8_t iface = 0;
    uint8_t report_len = 0;
    uint8_t report[16]{};
    uint32_t age_ms = 0;
    uint32_t duration_ms = 0;
    uint32_t report_count = 0;
    String error;
  };
  HidObserveResult runHidObserve(uint32_t wait_ms = 2000, bool clear = true);

private:
  bool ready_ = false;
  bool begin_ok_ = false;
  bool hint_printed_ = false;
  uint32_t started_ms_ = 0;
  uint32_t last_report_ms_ = 0;
  String last_event_json_;
  HostMscProbeResult last_probe_{};
  bool has_last_probe_ = false;

  // Cached DUT snapshot for /api/status
  bool dut_present_ = false;
  uint16_t dut_vid_ = 0;
  uint16_t dut_pid_ = 0;
  uint8_t dut_address_ = 0;
  String dut_mfr_;
  String dut_product_;
  String dut_serial_;
  String dut_speed_;
  uint8_t dut_cfg_ = 0;
  uint8_t dut_iface_count_ = 0;
  bool dut_hid_iface_ = false;

  // Last HID interrupt report (Host callback cache)
  volatile bool hid_armed_ = false;
  volatile bool hid_got_ = false;
  uint8_t hid_keycode_ = 0;
  uint8_t hid_ascii_ = 0;
  uint8_t hid_modifiers_ = 0;
  uint8_t hid_iface_ = 0;
  uint8_t hid_report_len_ = 0;
  uint8_t hid_report_[16]{};
  uint32_t hid_last_ms_ = 0;
  uint32_t hid_report_count_ = 0;

  void printNoDeviceHints() const;
  void clearDut();
  void noteHidReport(uint8_t keycode, uint8_t ascii, uint8_t modifiers,
                     uint8_t iface, const uint8_t *data, size_t len);
};

} // namespace usb_analyzer
