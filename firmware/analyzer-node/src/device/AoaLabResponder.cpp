#include "device/AoaLabResponder.h"

#if ANALYZER_ENABLE_USB_DEVICE

#include "capture/CaptureLog.h"
#include "device/UsbDeviceNode.h"
#include "tusb.h"

#include <cstring>

namespace usb_analyzer {

AoaLabResponder &AoaLabResponder::instance() {
  static AoaLabResponder r;
  return r;
}

void AoaLabResponder::setEnabled(bool on) {
  enabled_ = on;
  resetCounters();
  Serial.printf("[aoa] lab responder %s reenum_on_start=%d\n",
                on ? "ON" : "OFF", reenum_on_start_ ? 1 : 0);
}

void AoaLabResponder::resetCounters() {
  setup_count_ = aoa51_ = aoa52_ = aoa53_ = aoa51_ok_ = 0;
  string_bytes_ = 0;
  last_string_index_ = 0xff;
  last_string_[0] = '\0';
  string_scratch_len_ = 0;
  respond_in_flight_ = false;
  start_pending_ = false;
  memset(string_scratch_, 0, sizeof(string_scratch_));
}

bool AoaLabResponder::consumeStartPending() {
  if (!start_pending_) {
    return false;
  }
  start_pending_ = false;
  return true;
}

void AoaLabResponder::appendStatusJson(String &json) const {
  json += ",\"aoa\":{";
  json += "\"respond\":";
  json += enabled_ ? "true" : "false";
  json += ",\"reenumOnStart\":";
  json += reenum_on_start_ ? "true" : "false";
  json += ",\"setup\":";
  json += String(setup_count_);
  json += ",\"aoa51\":";
  json += String(aoa51_);
  json += ",\"aoa52\":";
  json += String(aoa52_);
  json += ",\"aoa53\":";
  json += String(aoa53_);
  json += ",\"aoa51Ok\":";
  json += String(aoa51_ok_);
  json += ",\"stringBytes\":";
  json += String(string_bytes_);
  json += ",\"lastStringIndex\":";
  json += String(last_string_index_);
  if (last_string_[0]) {
    json += ",\"lastString\":\"";
    json += last_string_;
    json += "\"";
  }
  json += ",\"claim\":\"aoa_control_handshake_lab\"";
  json += ",\"note\":\"not_hu_wire_not_bcl\"";
  json += "}";
}

bool AoaLabResponder::onControl(uint8_t rhport, uint8_t stage,
                                const void *request_void) {
  if (!enabled_ || !request_void) {
    return false;
  }
  auto const *request =
      reinterpret_cast<tusb_control_request_t const *>(request_void);

  const uint8_t bm = request->bmRequestType;
  const uint8_t br = request->bRequest;
  const uint16_t wv = request->wValue;
  const uint16_t wi = request->wIndex;
  const uint16_t wl = request->wLength;
  const bool vendor = ((bm >> 5) & 0x3) == 2;

  // DATA/ACK after successful SETUP respond — must not stall EP0
  // (hu-probe Lab: 51 Pipe error when complete returned false).
  if (stage != CONTROL_STAGE_SETUP && respond_in_flight_) {
    if (vendor && br == 52 && stage == CONTROL_STAGE_DATA &&
        string_scratch_len_ > 0) {
      const size_t n = string_scratch_len_ < sizeof(last_string_) - 1
                           ? string_scratch_len_
                           : sizeof(last_string_) - 1;
      memcpy(last_string_, string_scratch_, n);
      last_string_[n] = '\0';
      // Strip trailing NULs for display
      size_t end = n;
      while (end > 0 && last_string_[end - 1] == '\0') {
        end--;
      }
      last_string_[end] = '\0';
      string_bytes_ += string_scratch_len_;
      last_string_index_ = static_cast<uint8_t>(wi & 0xff);
      CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo, 0,
                                  string_scratch_len_, 52, wi, 0, "aoa52_str");
    }
    if (stage == CONTROL_STAGE_ACK) {
      respond_in_flight_ = false;
    }
    return true;
  }

  if (stage != CONTROL_STAGE_SETUP) {
    return false;
  }
  if (!vendor) {
    return false;
  }

  setup_count_++;
  bool handled = false;

  if (br == 51 && (bm & 0x80)) {
    // GET_PROTOCOL — IN|VENDOR, 2 bytes LE
    aoa51_++;
    handled = tud_control_xfer(rhport, request, &protocol_le_, 2);
    if (handled) {
      aoa51_ok_++;
    }
    CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo,
                                handled ? 0 : 1, 2, 51, protocol_le_, 0,
                                handled ? "aoa51_ok" : "aoa51_fail");
  } else if (br == 52 && !(bm & 0x80)) {
    // SEND_STRING — OUT|VENDOR, index = string ID
    aoa52_++;
    last_string_index_ = static_cast<uint8_t>(wi & 0xff);
    string_scratch_len_ =
        wl > sizeof(string_scratch_) ? sizeof(string_scratch_) : wl;
    memset(string_scratch_, 0, sizeof(string_scratch_));
    if (string_scratch_len_ == 0) {
      handled = tud_control_status(rhport, request);
    } else {
      handled =
          tud_control_xfer(rhport, request, string_scratch_, string_scratch_len_);
    }
    CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo,
                                handled ? 0 : 1, wl, 52, wi, 0,
                                handled ? "aoa52_ok" : "aoa52_fail");
  } else if (br == 53 && !(bm & 0x80)) {
    // START — OUT|VENDOR, no data
    aoa53_++;
    handled = tud_control_status(rhport, request);
    if (handled && reenum_on_start_) {
      start_pending_ = true;
    }
    CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo,
                                handled ? 0 : 1, 0, 53, 0, 0,
                                handled ? "aoa53_ok" : "aoa53_fail");
  } else if (br == 58 && !(bm & 0x80)) {
    // AOAv2 SET_AUDIO_MODE — accept status only (lab)
    handled = tud_control_status(rhport, request);
  }

  (void)wv;
  respond_in_flight_ = handled;
  return handled;
}

} // namespace usb_analyzer

// Strong override of weak stub in esp32-hal-tinyusb.c
extern "C" bool tinyusb_vendor_control_request_cb(
    uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  return usb_analyzer::AoaLabResponder::instance().onControl(rhport, stage,
                                                             request);
}

#else

namespace usb_analyzer {

AoaLabResponder &AoaLabResponder::instance() {
  static AoaLabResponder r;
  return r;
}
void AoaLabResponder::setEnabled(bool) {}
void AoaLabResponder::resetCounters() {}
bool AoaLabResponder::consumeStartPending() { return false; }
void AoaLabResponder::appendStatusJson(String &json) const {
  json += ",\"aoa\":{\"respond\":false}";
  (void)json;
}
bool AoaLabResponder::onControl(uint8_t, uint8_t, const void *) { return false; }

} // namespace usb_analyzer

#endif
