#include "device/UsbDeviceNode.h"

#if ANALYZER_ENABLE_USB_DEVICE

#include "USB.h"
#include "USBMSC.h"
#include "capture/CaptureLog.h"
#include "protocol/DataPlane.h"

#include <cstring>
#include <usb_analyzer/UsbLabPattern.h>

namespace usb_analyzer {
namespace {

USBMSC g_msc;
static uint8_t g_sector[512];
static UsbDeviceNode *g_self = nullptr;

int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  uint8_t *out = static_cast<uint8_t *>(buffer);
  if (g_self && g_self->proxyEnabled() &&
      g_self->fillFromProxy(lba, offset, out, bufsize)) {
    g_self->noteRead(bufsize);
    CaptureLog::instance().push(CapRole::Device, CapType::MscProxyRsp, 0, bufsize,
                                lba, bufsize / 512u, 0, "from_host_dut");
    return static_cast<int32_t>(bufsize);
  }
  // Prefer sectors previously written by Host (write cache), else lab pattern.
  uint32_t done = 0;
  while (done < bufsize) {
    const uint32_t abs_off = offset + done;
    const uint32_t cur_lba = lba + (abs_off / 512u);
    const uint32_t cur_off = abs_off % 512u;
    uint32_t n = bufsize - done;
    const uint32_t room = 512u - cur_off;
    if (n > room) {
      n = room;
    }
    uint8_t cached[512];
    if (g_self && cur_off == 0 && n == 512 &&
        g_self->loadCachedSector(cur_lba, cached)) {
      memcpy(out + done, cached, 512);
    } else if (g_self && g_self->loadCachedSector(cur_lba, cached)) {
      memcpy(out + done, cached + cur_off, n);
    } else {
      fillLabSector(out + done, n, cur_lba, cur_off);
    }
    done += n;
  }
  if (g_self) {
    g_self->noteRead(bufsize);
    if ((g_self->readOps() & 31u) == 1u) {
      CaptureLog::instance().push(CapRole::Device, CapType::MscRead, 0, bufsize,
                                  lba, bufsize / 512u, 0, "gadget_pattern");
      CaptureLog::instance().push(CapRole::Device, CapType::BotCbw, 0, 31, lba,
                                  bufsize / 512u, 0, "READ10_inferred");
      CaptureLog::instance().push(CapRole::Device, CapType::BotCsw, 0, 13, lba, 0,
                                  0, "CSW_ok_inferred");
    }
  }
  return static_cast<int32_t>(bufsize);
}

int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer,
                uint32_t bufsize) {
  if (!g_self) {
    return -1;
  }
  if (!g_self->storeWrite(lba, offset, buffer, bufsize)) {
    return -1;
  }
  g_self->noteWrite(bufsize);
  CaptureLog::instance().push(CapRole::Device, CapType::MscWrite, 0, bufsize, lba,
                              bufsize / 512u, 0, "gadget_write");
  CaptureLog::instance().push(CapRole::Device, CapType::BotCbw, 0, 31, lba,
                              bufsize / 512u, 0, "WRITE10_inferred");
  CaptureLog::instance().push(CapRole::Device, CapType::BotCsw, 0, 13, lba, 0, 0,
                              "CSW_ok_inferred");
  return static_cast<int32_t>(bufsize);
}

bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  Serial.printf("[device] start_stop power=%u start=%d eject=%d\n", power_condition,
                start ? 1 : 0, load_eject ? 1 : 0);
  return true;
}

void onEvent(void *arg, esp_event_base_t event_base, int32_t event_id,
             void *event_data) {
  (void)arg;
  (void)event_base;
  (void)event_data;
  if (!g_self) {
    return;
  }
  if (event_id == ARDUINO_USB_STARTED_EVENT) {
    g_self->notifyMounted(true);
    CaptureLog::instance().push(CapRole::Device, CapType::Attach, 0, 0, 0, 0, 0,
                                "usb_started");
    Serial.println("[device] USB STARTED (host configured / OTG mounted)");
  }
  if (event_id == ARDUINO_USB_STOPPED_EVENT) {
    g_self->notifyMounted(false);
    CaptureLog::instance().push(CapRole::Device, CapType::Detach, 0, 0, 0, 0, 0,
                                "usb_stopped");
    Serial.println("[device] USB STOPPED (OTG unmounted)");
  }
  if (event_id == ARDUINO_USB_SUSPEND_EVENT) {
    g_self->notifySuspended(true);
    Serial.println("[device] USB SUSPEND");
  }
  if (event_id == ARDUINO_USB_RESUME_EVENT) {
    g_self->notifySuspended(false);
    Serial.println("[device] USB RESUME");
  }
}

} // namespace

void UsbDeviceNode::noteRead(uint32_t bytes) {
  read_ops_++;
  read_bytes_ += bytes;
}

void UsbDeviceNode::noteWrite(uint32_t bytes) {
  write_ops_++;
  write_bytes_ += bytes;
}

bool UsbDeviceNode::storeWrite(uint32_t lba, uint32_t offset, const uint8_t *in,
                               uint32_t bufsize) {
  if (!in || bufsize == 0) {
    return false;
  }
  uint32_t done = 0;
  while (done < bufsize) {
    const uint32_t abs_off = offset + done;
    const uint32_t cur_lba = lba + (abs_off / 512u);
    const uint32_t cur_off = abs_off % 512u;
    uint32_t n = bufsize - done;
    const uint32_t room = 512u - cur_off;
    if (n > room) {
      n = room;
    }
    // Find or allocate cache entry for this LBA.
    WriteCacheEntry *slot = nullptr;
    for (size_t i = 0; i < kWriteCache; i++) {
      if (write_cache_[i].valid && write_cache_[i].lba == cur_lba) {
        slot = &write_cache_[i];
        break;
      }
    }
    if (!slot) {
      slot = &write_cache_[write_cache_i_ % kWriteCache];
      write_cache_i_++;
      slot->lba = cur_lba;
      slot->valid = true;
      fillLabSector(slot->data, 512, cur_lba, 0); // base then overlay
    }
    memcpy(slot->data + cur_off, in + done, n);
    done += n;
  }
  return true;
}

bool UsbDeviceNode::loadCachedSector(uint32_t lba, uint8_t *out512) const {
  if (!out512) {
    return false;
  }
  for (size_t i = 0; i < kWriteCache; i++) {
    if (write_cache_[i].valid && write_cache_[i].lba == lba) {
      memcpy(out512, write_cache_[i].data, 512);
      return true;
    }
  }
  return false;
}

void UsbDeviceNode::setProxy(DataPlane *plane, const String &peer_ip) {
  proxy_plane_ = plane;
  proxy_peer_ = peer_ip;
  proxy_on_ = plane && peer_ip.length() > 0;
  Serial.printf("[device] MSC proxy %s peer=%s\n", proxy_on_ ? "ON" : "OFF",
                proxy_peer_.c_str());
}

bool UsbDeviceNode::fillFromProxy(uint32_t lba, uint32_t offset, uint8_t *out,
                                  uint32_t bufsize) {
  if (!proxy_on_ || !proxy_plane_ || !out || bufsize == 0) {
    return false;
  }
  // Only full-sector aligned proxy fetches for Gate 4 slice.
  if (offset != 0 || (bufsize % 512u) != 0) {
    return false;
  }
  uint32_t blocks = bufsize / 512u;
  uint32_t done = 0;
  while (done < blocks) {
    uint32_t n = blocks - done;
    if (n > 8) {
      n = 8;
    }
    String err;
    if (!proxy_plane_->proxyMscRead(proxy_peer_, lba + done, n, out + done * 512u,
                                    n * 512u, &err)) {
      Serial.printf("[device] proxy fail lba=%u n=%u err=%s\n", lba + done, n,
                    err.c_str());
      return false;
    }
    done += n;
  }
  return true;
}

void UsbDeviceNode::appendStatusJson(String &json) const {
  json += ",\"gadget\":{";
  json += "\"readOps\":";
  json += String(read_ops_);
  json += ",\"readBytes\":";
  json += String(read_bytes_);
  json += ",\"writeOps\":";
  json += String(write_ops_);
  json += ",\"writeBytes\":";
  json += String(write_bytes_);
  json += ",\"pattern\":\"lab_sector_v1\"";
  json += ",\"proxy\":";
  json += proxy_on_ ? "true" : "false";
  json += ",\"proxyPeer\":\"";
  json += proxy_peer_;
  json += "\"";
  json += "}";
}

void UsbDeviceNode::notifyMounted(bool on) {
  mounted_ = on;
  if (!on) {
    suspended_ = false;
  }
}
void UsbDeviceNode::notifySuspended(bool on) { suspended_ = on; }

const char *UsbDeviceNode::otgState() const {
  if (!begin_ok_) {
    return "disabled";
  }
  if (mounted_ && suspended_) {
    return "suspended";
  }
  if (mounted_) {
    return "connected";
  }
  if (USB) {
    return "connected";
  }
  return "waiting";
}

bool UsbDeviceNode::begin() {
  g_self = this;
  Serial.println("[device] TinyUSB MSC gadget bring-up (Gate 2)");
  Serial.println(
      "[device] CLAIM: gadget present to host — not wire capture, not MITM");
  Serial.println(
      "[device] Debug: UART/COM only; OTG port is the gadget toward Host/PC");
  Serial.println("[device] MSC pattern=lab_sector_v1 for OTG-loop probes");

  USB.onEvent(onEvent);
  g_msc.vendorID("MPB");
  g_msc.productID("ANALYZER");
  g_msc.productRevision("0001");
  g_msc.onRead(onRead);
  g_msc.onWrite(onWrite);
  g_msc.onStartStop(onStartStop);
  g_msc.mediaPresent(true);
  g_msc.isWritable(true);
  g_msc.begin(512, 512);

  USB.begin();
  begin_ok_ = true;
  ready_ = true;
  mounted_ = static_cast<bool>(USB);
  Serial.println("[device] USB.begin() issued — wait for host enumeration");
  Serial.printf("[device] otg_state=%s\n", otgState());
  (void)g_sector;
  return begin_ok_;
}

void UsbDeviceNode::loop() {
  if (begin_ok_) {
    const bool now = static_cast<bool>(USB);
    if (now != mounted_) {
      mounted_ = now;
      Serial.printf("[device] otg_state → %s\n", otgState());
    }
  }
}

} // namespace usb_analyzer

#else // !ANALYZER_ENABLE_USB_DEVICE

namespace usb_analyzer {

bool UsbDeviceNode::begin() {
  Serial.println("[device] ANALYZER_ENABLE_USB_DEVICE=0 — stub");
  return false;
}
void UsbDeviceNode::loop() {}
const char *UsbDeviceNode::otgState() const { return "disabled"; }
void UsbDeviceNode::notifyMounted(bool) {}
void UsbDeviceNode::notifySuspended(bool) {}
void UsbDeviceNode::noteRead(uint32_t) {}
void UsbDeviceNode::noteWrite(uint32_t) {}
void UsbDeviceNode::setProxy(DataPlane *, const String &) {}
bool UsbDeviceNode::fillFromProxy(uint32_t, uint32_t, uint8_t *, uint32_t) {
  return false;
}
bool UsbDeviceNode::storeWrite(uint32_t, uint32_t, const uint8_t *, uint32_t) {
  return false;
}
bool UsbDeviceNode::loadCachedSector(uint32_t, uint8_t *) const { return false; }
void UsbDeviceNode::appendStatusJson(String &json) const {
  json += ",\"gadget\":{\"readOps\":0,\"readBytes\":0,\"writeOps\":0,\"proxy\":false}";
  (void)json;
}

} // namespace usb_analyzer

#endif
