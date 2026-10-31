#pragma once

/**
 * Gate 5 slice — L1/lab capture ring (not D+/D− wire).
 * Emits capture_event.v1 JSON via /api/capture.
 */

#include <Arduino.h>

namespace usb_analyzer {

enum class CapRole : uint8_t { Host = 1, Device = 2, Plane = 3 };
enum class CapType : uint8_t {
  Attach = 1,
  Detach = 2,
  EnumInfo = 3,
  MscRead = 4,
  MscProxyReq = 5,
  MscProxyRsp = 6,
  TcpBulk = 7,
  Error = 8,
  Overflow = 9,
  SyncMarker = 10,
};

struct CapEvent {
  uint32_t seq = 0;
  uint32_t timestamp_us = 0;
  CapRole role = CapRole::Host;
  CapType type = CapType::SyncMarker;
  uint8_t status = 0; // 0=ok
  uint8_t reserved = 0;
  uint32_t length = 0;
  uint32_t arg0 = 0; // e.g. lba / vid
  uint32_t arg1 = 0; // e.g. blocks / pid
  uint32_t duration_us = 0;
  char note[24] = {0};
};

class CaptureLog {
public:
  static constexpr size_t kCapacity = 256;

  static CaptureLog &instance();

  void begin(const char *node_id, const char *role_name);
  void setSession(const String &session_id);
  void setRoleName(const char *role_name);

  void push(CapRole role, CapType type, uint8_t status, uint32_t length,
            uint32_t arg0 = 0, uint32_t arg1 = 0, uint32_t duration_us = 0,
            const char *note = nullptr);

  uint32_t seq() const { return seq_; }
  uint32_t dropped() const { return dropped_; }
  uint32_t stored() const;
  bool overflowed() const { return overflowed_; }

  /** Append capture{} summary into status JSON. */
  void appendStatusJson(String &json) const;

  /**
   * Build JSON batch: {"version":1,"events":[...],"dropped":N,"next_seq":M}
   * @param since_seq exclusive lower bound (0 = from oldest retained)
   */
  String buildBatchJson(uint32_t since_seq, uint32_t limit) const;

private:
  CaptureLog() = default;

  CapEvent ring_[kCapacity];
  size_t head_ = 0; // next write
  size_t count_ = 0;
  uint32_t seq_ = 0;
  uint32_t dropped_ = 0;
  bool overflowed_ = false;
  String session_id_ = "lab-local";
  String node_id_;
  String role_name_ = "unassigned";

  static const char *typeName(CapType t);
  static const char *roleName(CapRole r);
  void eventToJson(const CapEvent &e, String &out) const;
};

} // namespace usb_analyzer
