#pragma once

#include <Arduino.h>
#include <usb_analyzer/DutInfoProxy.h>

namespace usb_analyzer {

class DataPlane;

/**
 * Gate 2: USB Device (gadget) bring-up.
 * Gate 4: MSC proxy + DUT-snapshot identity apply (not USB control MITM).
 */
class UsbDeviceNode {
public:
  bool begin();
  void loop();
  bool ready() const { return ready_; }
  bool beginOk() const { return begin_ok_; }

  /** TinyUSB configured by a USB host — OTG cable live as gadget. */
  bool otgMounted() const { return mounted_; }
  bool otgSuspended() const { return suspended_; }
  uint32_t suspendCount() const { return suspend_count_; }
  uint32_t resumeCount() const { return resume_count_; }
  const char *otgState() const;
  void notifyMounted(bool on);
  void notifySuspended(bool on);

  /** When enabled, onRead/onWrite go to peer Host DUT over WiFi (MITM path). */
  /** @param skip_identity  true → no DUT snapshot / soft-reenum (OTG-loop lab). */
  void setProxy(DataPlane *plane, const String &peer_ip,
                bool skip_identity = false);
  bool proxyEnabled() const { return proxy_on_; }
  bool proxySkipIdentity() const { return proxy_skip_identity_; }
  const String &proxyPeer() const { return proxy_peer_; }
  bool refreshProxiedDut();
  bool hasProxiedDut() const { return proxied_dut_.present != 0; }
  const DutInfoSnapshot &proxiedDut() const { return proxied_dut_; }
  bool identityApplied() const { return identity_applied_; }

  void noteRead(uint32_t bytes);
  void noteWrite(uint32_t bytes);
  void noteProxyHit(uint32_t bytes);
  void noteProxyFail();
  uint32_t readOps() const { return read_ops_; }
  uint32_t readBytes() const { return read_bytes_; }
  uint32_t writeOps() const { return write_ops_; }
  uint32_t writeBytes() const { return write_bytes_; }
  uint32_t proxyHits() const { return proxy_hits_; }
  uint32_t proxyFails() const { return proxy_fails_; }
  uint32_t proxyBytes() const { return proxy_bytes_; }
  void appendStatusJson(String &json) const;

  /** Lab selftest: Device→Host DUT MSC READ via armed proxy (not PC path). */
  bool selftestProxyRead(uint32_t lba, uint32_t blocks, uint8_t *out,
                         String *err);

  /** Copy current gadget device descriptor (18 B) for ctrl-forward compare. */
  bool copyLocalDeviceDescriptor(uint8_t *out18) const;
  /** ASCII view of string index 1=mfr 2=product 3=serial. */
  bool localStringAscii(uint8_t index, char *out, size_t out_len) const;

  /** Lab AOA EP0 responder (51/52/53) — see AoaLabResponder. */
  void setAoaRespond(bool on, bool reenum_on_start = true);
  bool aoaRespondEnabled() const;
  /** Soft-reenum as Google accessory VID/PID after START (MSC IF remains). */
  void applyAoaAccessoryIdentity();

  /** Lab soft disconnect/connect — Host sees Detach+Attach (not control MITM). */
  void requestSoftReenum();

  /**
   * Lab HID keyboard inject (interrupt IN). ASCII or raw HID usage.
   * Claim: hid_interrupt_lab_loop — NOT MITM / L0 / HU.
   */
  bool injectHidKey(uint8_t ascii_or_raw, bool raw = false);
  bool hidEnabled() const { return hid_enabled_; }
  uint32_t hidInjectCount() const { return hid_injects_; }

  /** Used from MSC callback (same TU). */
  bool fillFromProxy(uint32_t lba, uint32_t offset, uint8_t *out,
                     uint32_t bufsize);
  bool writeThroughProxy(uint32_t lba, uint32_t offset, const uint8_t *in,
                         uint32_t bufsize);
  bool storeWrite(uint32_t lba, uint32_t offset, const uint8_t *in,
                  uint32_t bufsize);
  bool loadCachedSector(uint32_t lba, uint8_t *out512) const;

private:
  void applyLabIdentity();
  bool applyDutIdentity(const DutInfoSnapshot &snap);
  void scheduleSoftReenum();
  void runSoftReenum();

  bool ready_ = false;
  bool begin_ok_ = false;
  bool mounted_ = false;
  bool suspended_ = false;
  uint32_t suspend_count_ = 0;
  uint32_t resume_count_ = 0;
  uint32_t read_ops_ = 0;
  uint32_t read_bytes_ = 0;
  uint32_t write_ops_ = 0;
  uint32_t write_bytes_ = 0;
  uint32_t proxy_hits_ = 0;
  uint32_t proxy_fails_ = 0;
  uint32_t proxy_bytes_ = 0;
  DataPlane *proxy_plane_ = nullptr;
  String proxy_peer_;
  bool proxy_on_ = false;
  bool proxy_skip_identity_ = false;
  DutInfoSnapshot proxied_dut_{};
  bool identity_applied_ = false;
  bool pending_reenum_ = false;
  uint32_t reenum_at_ms_ = 0;
  uint32_t gadget_blocks_ = 512;
  uint16_t gadget_block_size_ = 512;
  uint16_t applied_vid_ = 0;
  uint16_t applied_pid_ = 0;
  bool hid_enabled_ = false;
  uint32_t hid_injects_ = 0;

  static constexpr size_t kWriteCache = 64; // covers typical lab write probe
  struct WriteCacheEntry {
    uint32_t lba = 0;
    bool valid = false;
    uint8_t data[512];
  };
  WriteCacheEntry write_cache_[kWriteCache]{};
  size_t write_cache_i_ = 0;
};

} // namespace usb_analyzer
