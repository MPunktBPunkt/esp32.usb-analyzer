#include "device/AoaLabResponder.h"

#if ANALYZER_ENABLE_USB_DEVICE

#include "USBVendor.h"
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
  Serial.printf("[aoa] lab responder %s reenum=%d bulk_echo=%d\n",
                on ? "ON" : "OFF", reenum_on_start_ ? 1 : 0,
                bulk_echo_ ? 1 : 0);
}

void AoaLabResponder::resetCounters() {
  setup_count_ = aoa51_ = aoa52_ = aoa53_ = aoa51_ok_ = 0;
  string_bytes_ = bulk_rx_ = bulk_tx_ = bulk_fifo_ = bulk_polls_ = 0;
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
  json += ",\"bulkEcho\":";
  json += bulk_echo_ ? "true" : "false";
  json += ",\"vendorMounted\":";
  json += (vendor_ && vendor_->mounted()) ? "true" : "false";
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
  json += ",\"bulkRx\":";
  json += String(bulk_rx_);
  json += ",\"bulkTx\":";
  json += String(bulk_tx_);
  json += ",\"bulkFifo\":";
  json += String(bulk_fifo_);
  json += ",\"bulkPolls\":";
  json += String(bulk_polls_);
  json += ",\"tudVendorAvail\":";
  json += String(static_cast<unsigned>(tud_vendor_n_available(0)));
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

void AoaLabResponder::pollBulkEcho() {
  // Echo via TinyUSB vendor FIFO first (bypass Arduino RX queue), then USBVendor.
  if (!bulk_echo_) {
    return;
  }
  bulk_polls_++;
  uint8_t buf[64];

  // Path A: TinyUSB class FIFO (itf 0) — works even if USBVendor queue misses.
  if (tud_vendor_n_mounted(0)) {
    uint32_t avail = tud_vendor_n_available(0);
    if (avail > bulk_fifo_) {
      bulk_fifo_ = avail;
    }
    int guard = 0;
    while (avail > 0 && guard++ < 8) {
      const uint32_t n = tud_vendor_n_read(0, buf, sizeof(buf));
      if (n == 0) {
        break;
      }
      bulk_rx_ += n;
      uint32_t w = 0;
      if (tud_vendor_n_write_available(0) >= n) {
        w = tud_vendor_n_write(0, buf, n);
        tud_vendor_n_write_flush(0);
      }
      bulk_tx_ += w;
      CaptureLog::instance().push(CapRole::Device, CapType::TcpBulk,
                                  (w == n) ? 0 : 1, n, w, 0, 0,
                                  (w == n) ? "aoa_tud_echo" : "aoa_tud_short");
      avail = tud_vendor_n_available(0);
    }
  }

  // Path B: Arduino USBVendor queue (DATA event / Stream API)
  if (!vendor_ || !vendor_->mounted()) {
    return;
  }
  int guard = 0;
  while (vendor_->available() > 0 && guard++ < 8) {
    const int n = vendor_->read(buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    bulk_rx_ += static_cast<uint32_t>(n);
    const size_t w = vendor_->write(buf, static_cast<size_t>(n));
    bulk_tx_ += static_cast<uint32_t>(w);
    CaptureLog::instance().push(CapRole::Device, CapType::TcpBulk,
                                (w == static_cast<size_t>(n)) ? 0 : 1,
                                static_cast<uint32_t>(n),
                                static_cast<uint32_t>(w), 0, 0,
                                (w == static_cast<size_t>(n)) ? "aoa_bulk_echo"
                                                              : "aoa_bulk_short");
  }
}

bool AoaLabResponder::onVendorRequest(uint8_t rhport, uint8_t stage,
                                      const void *request_void) {
  if (!enabled_ || !request_void || !vendor_) {
    return false;
  }
  auto const *request =
      reinterpret_cast<arduino_usb_control_request_t const *>(request_void);

  const uint8_t bm = static_cast<uint8_t>(
      (request->bmRequestDirection << 7) | (request->bmRequestType << 5) |
      request->bmRequestRecipient);
  const uint8_t br = request->bRequest;
  const uint16_t wi = request->wIndex;
  const uint16_t wl = request->wLength;
  const bool vendor_type = ((bm >> 5) & 0x3) == 2;

  // DATA/ACK after SETUP respond — must return true (hu-probe Lab 51 Pipe fix)
  if (stage != CONTROL_STAGE_SETUP && respond_in_flight_) {
    if (vendor_type && br == 52 && stage == CONTROL_STAGE_DATA &&
        string_scratch_len_ > 0) {
      size_t n = string_scratch_len_ < sizeof(last_string_) - 1
                     ? string_scratch_len_
                     : sizeof(last_string_) - 1;
      memcpy(last_string_, string_scratch_, n);
      last_string_[n] = '\0';
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

  if (stage != CONTROL_STAGE_SETUP || !vendor_type) {
    return false;
  }

  setup_count_++;
  bool handled = false;

  if (br == 51 && (bm & 0x80)) {
    aoa51_++;
    handled = vendor_->sendResponse(rhport, request, &protocol_le_, 2);
    if (handled) {
      aoa51_ok_++;
    }
    CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo,
                                handled ? 0 : 1, 2, 51, protocol_le_, 0,
                                handled ? "aoa51_ok" : "aoa51_fail");
  } else if (br == 52 && !(bm & 0x80)) {
    aoa52_++;
    last_string_index_ = static_cast<uint8_t>(wi & 0xff);
    string_scratch_len_ =
        wl > sizeof(string_scratch_) ? sizeof(string_scratch_) : wl;
    memset(string_scratch_, 0, sizeof(string_scratch_));
    if (string_scratch_len_ == 0) {
      handled = vendor_->sendResponse(rhport, request, nullptr, 0);
    } else {
      handled = vendor_->sendResponse(rhport, request, string_scratch_,
                                      string_scratch_len_);
    }
    CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo,
                                handled ? 0 : 1, wl, 52, wi, 0,
                                handled ? "aoa52_ok" : "aoa52_fail");
  } else if (br == 53 && !(bm & 0x80)) {
    aoa53_++;
    handled = vendor_->sendResponse(rhport, request, nullptr, 0);
    if (handled && reenum_on_start_) {
      start_pending_ = true;
    }
    CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo,
                                handled ? 0 : 1, 0, 53, 0, 0,
                                handled ? "aoa53_ok" : "aoa53_fail");
  } else if (br == 58 && !(bm & 0x80)) {
    handled = vendor_->sendResponse(rhport, request, nullptr, 0);
  }

  respond_in_flight_ = handled;
  return handled;
}

} // namespace usb_analyzer

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
void AoaLabResponder::pollBulkEcho() {}
bool AoaLabResponder::onVendorRequest(uint8_t, uint8_t, const void *) {
  return false;
}

} // namespace usb_analyzer

#endif
