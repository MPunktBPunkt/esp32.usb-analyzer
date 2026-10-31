#include "capture/CaptureLog.h"

#include <cstring>
#include <esp_timer.h>

namespace usb_analyzer {

CaptureLog &CaptureLog::instance() {
  static CaptureLog log;
  return log;
}

void CaptureLog::begin(const char *node_id, const char *role_name) {
  node_id_ = node_id ? node_id : "usb-analyzer";
  role_name_ = role_name ? role_name : "unassigned";
  session_id_ = "lab-" + String(static_cast<unsigned long>(millis()));
  push(CapRole::Plane, CapType::SyncMarker, 0, 0, 0, 0, 0, "boot");
}

void CaptureLog::setSession(const String &session_id) {
  if (session_id.length()) {
    session_id_ = session_id;
  }
}

void CaptureLog::setRoleName(const char *role_name) {
  if (role_name) {
    role_name_ = role_name;
  }
}

uint32_t CaptureLog::stored() const { return static_cast<uint32_t>(count_); }

void CaptureLog::push(CapRole role, CapType type, uint8_t status, uint32_t length,
                      uint32_t arg0, uint32_t arg1, uint32_t duration_us,
                      const char *note) {
  pushPayload(role, type, status, length, arg0, arg1, duration_us, note, nullptr,
              0);
}

void CaptureLog::pushPayload(CapRole role, CapType type, uint8_t status,
                             uint32_t length, uint32_t arg0, uint32_t arg1,
                             uint32_t duration_us, const char *note,
                             const uint8_t *payload, uint8_t payload_len) {
  CapEvent e;
  e.seq = ++seq_;
  e.timestamp_us = static_cast<uint32_t>(esp_timer_get_time());
  e.role = role;
  e.type = type;
  e.status = status;
  e.length = length;
  e.arg0 = arg0;
  e.arg1 = arg1;
  e.duration_us = duration_us;
  if (note) {
    strncpy(e.note, note, sizeof(e.note) - 1);
    e.note[sizeof(e.note) - 1] = 0;
  }
  if (payload && payload_len) {
    if (payload_len > sizeof(e.payload)) {
      payload_len = sizeof(e.payload);
    }
    memcpy(e.payload, payload, payload_len);
    e.payload_len = payload_len;
  }

  ring_[head_] = e;
  head_ = (head_ + 1) % kCapacity;
  if (count_ < kCapacity) {
    count_++;
  } else {
    dropped_++;
    overflowed_ = true;
  }
}

const char *CaptureLog::typeName(CapType t) {
  switch (t) {
  case CapType::Attach:
    return "attach";
  case CapType::Detach:
    return "detach";
  case CapType::EnumInfo:
    return "enum_info";
  case CapType::MscRead:
    return "msc_read";
  case CapType::MscWrite:
    return "msc_write";
  case CapType::MscProxyReq:
    return "msc_proxy_req";
  case CapType::MscProxyRsp:
    return "msc_proxy_rsp";
  case CapType::TcpBulk:
    return "tcp_bulk";
  case CapType::BotCbw:
    return "bot_cbw";
  case CapType::BotCsw:
    return "bot_csw";
  case CapType::Error:
    return "error";
  case CapType::Overflow:
    return "overflow";
  case CapType::SyncMarker:
    return "sync_marker";
  default:
    return "unknown";
  }
}

const char *CaptureLog::roleName(CapRole r) {
  switch (r) {
  case CapRole::Host:
    return "host";
  case CapRole::Device:
    return "device";
  case CapRole::Plane:
    return "plane";
  default:
    return "unknown";
  }
}

void CaptureLog::eventToJson(const CapEvent &e, String &out) const {
  out += "{";
  out += "\"version\":1";
  out += ",\"session_id\":\"" + session_id_ + "\"";
  out += ",\"node_id\":\"" + node_id_ + "\"";
  out += ",\"seq\":" + String(e.seq);
  out += ",\"timestamp_us\":" + String(e.timestamp_us);
  out += ",\"timestamp_domain\":\"local\"";
  out += ",\"role\":\"" + String(roleName(e.role)) + "\"";
  out += ",\"event_type\":\"" + String(typeName(e.type)) + "\"";
  out += ",\"status\":\"" + String(e.status == 0 ? "ok" : "error") + "\"";
  out += ",\"length\":" + String(e.length);
  out += ",\"duration_us\":" + String(e.duration_us);
  out += ",\"transfer_type\":\"bulk\"";
  out += ",\"layer\":\"L1_lab\"";
  out += ",\"claim\":\"not_wire_phy\"";
  const bool note_write =
      e.note[0] && (strstr(e.note, "WRITE") != nullptr ||
                    strstr(e.note, "write") != nullptr);
  const bool from_hook =
      e.note[0] && strstr(e.note, "class_hook") != nullptr;
  if (e.type == CapType::BotCbw && from_hook && e.payload_len >= 15) {
    const uint8_t op = e.payload[15]; // CBW.command[0]
    const bool is_write = (op == 0x2A);
    out += is_write ? ",\"direction\":\"out\"" : ",\"direction\":\"in\"";
    out += ",\"decoded\":{\"protocol\":\"msc-bot\",\"command\":\"";
    out += (op == 0x28) ? "READ_10" : (op == 0x2A) ? "WRITE_10" : "SCSI";
    out += "\"";
    out += ",\"scsi_opcode\":" + String(op);
    out += ",\"lba\":" + String(e.arg0);
    out += ",\"transfer_length\":" + String(e.arg1);
    out += ",\"signature\":\"USBC\"";
    out += ",\"source\":\"class_hook_mscd_xfer\"";
    out += "}";
  } else if (e.type == CapType::BotCsw && from_hook) {
    out += ",\"direction\":\"in\"";
    out += ",\"decoded\":{\"protocol\":\"msc-bot\",\"command\":\"CSW\"";
    out += ",\"residue\":" + String(e.arg1);
    out += ",\"csw_status\":" + String(e.arg0);
    out += ",\"signature\":\"USBS\"";
    out += ",\"source\":\"class_hook_mscd_xfer\"";
    out += "}";
  } else if (e.type == CapType::MscWrite ||
             (e.type == CapType::BotCbw && note_write)) {
    out += ",\"direction\":\"out\"";
    out += ",\"decoded\":{\"protocol\":\"msc-bot\",\"command\":\"WRITE_10\"";
    out += ",\"scsi_opcode\":42"; // 0x2A — decimal for valid JSON
    out += ",\"lba\":" + String(e.arg0);
    out += ",\"transfer_length\":" + String(e.arg1);
    out += ",\"source\":\"class_api_inferred\"";
    out += "}";
  } else if (e.type == CapType::MscRead || e.type == CapType::MscProxyReq ||
             e.type == CapType::MscProxyRsp || e.type == CapType::BotCbw) {
    out += ",\"direction\":\"in\"";
    out += ",\"decoded\":{\"protocol\":\"msc-bot\",\"command\":\"READ_10\"";
    out += ",\"scsi_opcode\":40"; // 0x28
    out += ",\"lba\":" + String(e.arg0);
    out += ",\"transfer_length\":" + String(e.arg1);
    out += ",\"source\":\"class_api_inferred\"";
    out += "}";
  } else if (e.type == CapType::BotCsw) {
    out += ",\"direction\":\"in\"";
    out += ",\"decoded\":{\"protocol\":\"msc-bot\",\"command\":\"CSW\"";
    out += ",\"lba\":" + String(e.arg0);
    out += ",\"residue\":" + String(e.arg1);
    const bool from_complete =
        e.note[0] && (strstr(e.note, "complete") != nullptr ||
                      strstr(e.note, "_done") != nullptr);
    out += from_complete ? ",\"source\":\"class_complete_cb\""
                         : ",\"source\":\"class_api_inferred\"";
    out += "}";
  } else if (e.type == CapType::Attach || e.type == CapType::EnumInfo) {
    out += ",\"decoded\":{\"vid\":" + String(e.arg0);
    out += ",\"pid\":" + String(e.arg1);
    if (e.type == CapType::EnumInfo && e.length) {
      out += ",\"address\":" + String(e.length);
    }
    if (e.type == CapType::EnumInfo && e.note[0]) {
      out += ",\"product\":\"";
      out += e.note;
      out += "\"";
    }
    if (e.type == CapType::Attach && e.note[0]) {
      out += ",\"speed\":\"";
      out += e.note;
      out += "\"";
    }
    out += "}";
  }
  if (e.note[0]) {
    out += ",\"note\":\"";
    out += e.note;
    out += "\"";
  }
  if (e.payload_len) {
    out += ",\"payload_hex\":\"";
    for (uint8_t i = 0; i < e.payload_len; i++) {
      char hx[3];
      snprintf(hx, sizeof(hx), "%02x", e.payload[i]);
      out += hx;
    }
    out += "\"";
  }
  out += "}";
}

void CaptureLog::appendStatusJson(String &json) const {
  json += ",\"capture\":{";
  json += "\"seq\":" + String(seq_);
  json += ",\"stored\":" + String(stored());
  json += ",\"capacity\":" + String(static_cast<unsigned>(kCapacity));
  json += ",\"dropped\":" + String(dropped_);
  json += ",\"overflow\":";
  json += overflowed_ ? "true" : "false";
  json += ",\"session\":\"" + session_id_ + "\"";
  json += ",\"layer\":\"L1_lab\"";
  json += ",\"wire\":false";
  json += "}";
}

String CaptureLog::buildBatchJson(uint32_t since_seq, uint32_t limit) const {
  if (limit == 0 || limit > 128) {
    limit = 64;
  }
  String out = "{";
  out += "\"version\":1";
  out += ",\"msg\":\"capture_batch\"";
  out += ",\"session_id\":\"" + session_id_ + "\"";
  out += ",\"node_id\":\"" + node_id_ + "\"";
  out += ",\"dropped\":" + String(dropped_);
  out += ",\"overflow\":";
  out += overflowed_ ? "true" : "false";
  out += ",\"next_seq\":" + String(seq_ + 1);
  out += ",\"layer\":\"L1_lab\"";
  out += ",\"note\":\"Stack/lab events — not D+/D- wire capture\"";
  out += ",\"events\":[";

  // Walk from oldest retained to newest
  uint32_t emitted = 0;
  bool first = true;
  for (size_t i = 0; i < count_; i++) {
    const size_t idx =
        (head_ + kCapacity - count_ + i) % kCapacity;
    const CapEvent &e = ring_[idx];
    if (e.seq <= since_seq) {
      continue;
    }
    if (!first) {
      out += ",";
    }
    first = false;
    eventToJson(e, out);
    emitted++;
    if (emitted >= limit) {
      break;
    }
  }
  out += "]}";
  return out;
}

} // namespace usb_analyzer
