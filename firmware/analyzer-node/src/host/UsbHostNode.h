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

  /**
   * Host root-port Suspend/Resume via DWC HPRT (lab).
   * Claim: usb_bus_suspend_resume_lab — stack HPRT, NOT L0 PHY / MITM.
   */
  struct SuspendResumeResult {
    bool ok = false;
    bool suspended = false;
    bool resumed = false;
    bool post_traffic_ok = false;
    uint32_t hold_ms = 0;
    uint32_t duration_ms = 0;
    String error;
  };
  SuspendResumeResult runBusSuspend(uint32_t hold_ms = 100);
  SuspendResumeResult runBusResume();
  /** Suspend → hold → resume → GET_DESCRIPTOR smoke. */
  SuspendResumeResult runBusSuspendResume(uint32_t hold_ms = 100);

  /**
   * EP0 negative / robustness series (caller-initiated).
   * Claim: usb_ctrl_neg_lab — NOT transparent SETUP MITM.
   */
  struct CtrlCaseResult {
    const char *name = "";
    bool ok = false;       // transfer completed
    bool expect_ok = true; // whether we expected success
    bool pass = false;     // ok==expect_ok
    uint16_t actual = 0;
    int host_err = 0;
  };
  struct CtrlNegResult {
    bool ok = false;
    uint8_t cases = 0;
    uint8_t passed = 0;
    CtrlCaseResult detail[8]{};
    uint32_t duration_ms = 0;
    String error;
  };
  CtrlNegResult runCtrlNegSeries();

  /**
   * Gate 10A feasibility: DWC HPRT Level-2 regs + D+/D− pin ownership notes.
   * Claim: gate10a_feasibility — NOT live L0 packet capture / NOT Path A sniffer.
   */
  struct L0FeasibilityResult {
    bool ok = false;
    bool host_begun = false;
    bool dut_present = false;
    uint32_t hprt_raw = 0;
    bool prt_conn = false;
    bool prt_ena = false;
    bool prt_susp = false;
    bool prt_overcur = false;
    uint8_t prt_speed = 0; // 0=FS? DWC encoding — see note
    int gpio19_level = -1; // D−
    int gpio20_level = -1; // D+
    bool path_a_sniffer_ok = false; // always false — falsified
    String path_a_reason;
    String note;
    String error;
  };
  L0FeasibilityResult runL0Feasibility();

  /**
   * Experiment E1.1 / Gate 10A hardening: poll GPIO19/20 (D−/D+) while OTG
   * owns the PHY, optionally with concurrent MSC traffic on the other core.
   * Measures sample rate, poll jitter, edge counts, and bus stability.
   * Claim: gate10a_e11_gpio_sample — NOT usable FS L0 capture.
   */
  struct E11GpioSampleResult {
    bool ok = false;
    bool host_begun = false;
    bool dut_before = false;
    bool dut_after = false;
    bool prt_conn_before = false;
    bool prt_conn_after = false;
    bool prt_ena_before = false;
    bool prt_ena_after = false;
    uint32_t hprt_before = 0;
    uint32_t hprt_after = 0;
    uint32_t samples = 0;
    uint32_t duration_us = 0;
    uint32_t rate_ksps = 0; // thousand samples / second
    uint32_t transitions_dm = 0;
    uint32_t transitions_dp = 0;
    uint32_t sample_dt_min_cy = 0;
    uint32_t sample_dt_max_cy = 0;
    uint32_t sample_dt_avg_cy = 0;
    uint32_t cpu_mhz = 0;
    uint32_t jitter_pp_ns = 0; // peak-to-peak poll period estimate
    bool traffic_ok = false;
    uint32_t traffic_rate_kBs = 0;
    bool post_probe_ok = false;
    bool bus_stable = false;
    bool destructive = false;
    bool meaningful_edges = false;
    bool path_a_usable = false;
    String path_a_reason;
    String note;
    String error;
    uint32_t duration_ms = 0;
  };
  E11GpioSampleResult runE11GpioSample(uint32_t samples = 200000,
                                       uint32_t traffic_blocks = 32);

  /**
   * Stop EspUsbHost so GPIO19/20 can be claimed for wire stimulus.
   * Irreversible until reboot (or future re-begin).
   */
  bool releaseHostStack(String &err);
  bool hostStackReleased() const { return host_released_; }

  /**
   * Gate 9: MSC READ under Device fault + REQUEST_SENSE + recovery probe.
   * Claim: gate9_fault_observe_lab — class/BOT path, NOT L0 wire.
   */
  struct FaultObserveResult {
    bool ok = false; // overall lab ran (even if read failed as expected)
    bool read_ok = false;
    bool sense_ok = false;
    bool recovery_ok = false;
    uint8_t sense_key = 0;
    uint8_t asc = 0;
    uint8_t asq = 0;
    uint8_t response_code = 0;
    uint32_t read_ms = 0;
    uint32_t recovery_ms = 0;
    uint32_t duration_ms = 0;
    String error;
  };
  FaultObserveResult runFaultObserve(uint32_t lba = 0, uint32_t blocks = 1,
                                     bool do_sense = true,
                                     bool do_recovery = true);

private:
  bool ready_ = false;
  bool begin_ok_ = false;
  bool host_released_ = false;
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
