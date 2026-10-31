#pragma once

#include <Arduino.h>
#include <usb_analyzer/DutInfoProxy.h>

namespace usb_analyzer {

class DataPlane;

/**
 * Gate 2: USB Device (gadget) bring-up.
 * Gate 4 slice: optional MSC proxy via DataPlane → Host DUT.
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
  const char *otgState() const;
  void notifyMounted(bool on);
  void notifySuspended(bool on);

  /** When enabled, onRead/onWrite go to peer Host DUT over WiFi (MITM path). */
  void setProxy(DataPlane *plane, const String &peer_ip);
  bool proxyEnabled() const { return proxy_on_; }
  const String &proxyPeer() const { return proxy_peer_; }
  bool refreshProxiedDut();
  bool hasProxiedDut() const { return proxied_dut_.present != 0; }
  const DutInfoSnapshot &proxiedDut() const { return proxied_dut_; }

  void noteRead(uint32_t bytes);
  void noteWrite(uint32_t bytes);
  uint32_t readOps() const { return read_ops_; }
  uint32_t readBytes() const { return read_bytes_; }
  uint32_t writeOps() const { return write_ops_; }
  uint32_t writeBytes() const { return write_bytes_; }
  void appendStatusJson(String &json) const;

  /** Used from MSC callback (same TU). */
  bool fillFromProxy(uint32_t lba, uint32_t offset, uint8_t *out,
                     uint32_t bufsize);
  bool writeThroughProxy(uint32_t lba, uint32_t offset, const uint8_t *in,
                         uint32_t bufsize);
  bool storeWrite(uint32_t lba, uint32_t offset, const uint8_t *in,
                  uint32_t bufsize);
  bool loadCachedSector(uint32_t lba, uint8_t *out512) const;

private:
  bool ready_ = false;
  bool begin_ok_ = false;
  bool mounted_ = false;
  bool suspended_ = false;
  uint32_t read_ops_ = 0;
  uint32_t read_bytes_ = 0;
  uint32_t write_ops_ = 0;
  uint32_t write_bytes_ = 0;
  DataPlane *proxy_plane_ = nullptr;
  String proxy_peer_;
  bool proxy_on_ = false;
  DutInfoSnapshot proxied_dut_{};

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
