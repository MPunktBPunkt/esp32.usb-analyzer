#include "device/UsbDeviceNode.h"

#if ANALYZER_ENABLE_USB_DEVICE

#include "USB.h"
#include "USBMSC.h"
#include "capture/CaptureLog.h"
#include "device/AoaLabResponder.h"
#include "esp32-hal-tinyusb.h"
#include "protocol/DataPlane.h"
#include "tusb.h"

#include <cstring>
#include <usb_analyzer/UsbLabPattern.h>

namespace usb_analyzer {
namespace {

USBMSC g_msc;
static uint8_t g_sector[512];
static UsbDeviceNode *g_self = nullptr;

// Own device/string descriptors so we can re-ident after USB.begin()
// (Arduino ESPUSB has no end(); VID/PID setters no-op once started).
static tusb_desc_device_t g_dev_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDOINT0_SIZE,
    .idVendor = USB_ESPRESSIF_VID,
    .idProduct = 0x1001,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static char g_str_mfr[64] = "MPB";
static char g_str_product[64] = "ANALYZER";
static char g_str_serial[64] = "lab-gadget";

static void copyField(char *dst, size_t dst_len, const char *src,
                      const char *fallback) {
  if (!dst || dst_len == 0) {
    return;
  }
  const char *s = (src && src[0]) ? src : fallback;
  strncpy(dst, s ? s : "", dst_len - 1);
  dst[dst_len - 1] = '\0';
}

static void setMscInquiry(const char *vendor8, const char *product16,
                          const char *rev4) {
  char v[9] = {0};
  char p[17] = {0};
  char r[5] = {0};
  copyField(v, sizeof(v), vendor8, "MPB");
  copyField(p, sizeof(p), product16, "ANALYZER");
  copyField(r, sizeof(r), rev4, "0001");
  g_msc.vendorID(v);
  g_msc.productID(p);
  g_msc.productRevision(r);
}

} // namespace

// Weak overrides — host re-reads these after tud_disconnect/connect.
extern "C" uint8_t const *tud_descriptor_device_cb(void) {
  return reinterpret_cast<uint8_t const *>(&g_dev_desc);
}

extern "C" uint16_t const *tud_descriptor_string_cb(uint8_t index,
                                                    uint16_t langid) {
  (void)langid;
  static uint16_t desc[64];
  if (index == 0) {
    desc[1] = 0x0409;
    desc[0] = (TUSB_DESC_STRING << 8) | (2 + 2);
    return desc;
  }
  const char *str = "MSC";
  if (index == 1) {
    str = g_str_mfr;
  } else if (index == 2) {
    str = g_str_product;
  } else if (index == 3) {
    str = g_str_serial;
  }
  size_t n = strlen(str);
  if (n > 62) {
    n = 62;
  }
  for (size_t i = 0; i < n; i++) {
    desc[1 + i] = static_cast<uint16_t>(static_cast<uint8_t>(str[i]));
  }
  desc[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 + n * 2));
  return desc;
}

namespace {

int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  uint8_t *out = static_cast<uint8_t *>(buffer);
  if (g_self && g_self->proxyEnabled()) {
    if (!g_self->fillFromProxy(lba, offset, out, bufsize)) {
      g_self->noteProxyFail();
      CaptureLog::instance().push(CapRole::Device, CapType::MscProxyRsp, 1,
                                  bufsize, lba, bufsize / 512u, 0, "proxy_fail");
      return -1; // SCSI fail — never fall back to lab pattern while proxy ON
    }
    g_self->noteRead(bufsize);
    g_self->noteProxyHit(bufsize);
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
      // Bot CBW/CSW come from TinyUSB class hooks (Gate-6), not App fiction.
      CaptureLog::instance().push(CapRole::Device, CapType::MscRead, 0, bufsize,
                                  lba, bufsize / 512u, 0, "gadget_pattern");
    }
  }
  return static_cast<int32_t>(bufsize);
}

int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer,
                uint32_t bufsize) {
  if (!g_self) {
    return -1;
  }
  if (g_self->proxyEnabled()) {
    if (!g_self->writeThroughProxy(lba, offset, buffer, bufsize)) {
      g_self->noteProxyFail();
      return -1;
    }
    g_self->noteProxyHit(bufsize);
  } else if (!g_self->storeWrite(lba, offset, buffer, bufsize)) {
    return -1;
  }
  g_self->noteWrite(bufsize);
  // Bot CBW/CSW come from TinyUSB class hooks (Gate-6), not App fiction.
  CaptureLog::instance().push(CapRole::Device, CapType::MscWrite, 0, bufsize, lba,
                              bufsize / 512u, 0,
                              g_self->proxyEnabled() ? "proxy_write" : "gadget_write");
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

void UsbDeviceNode::noteProxyHit(uint32_t bytes) {
  proxy_hits_++;
  proxy_bytes_ += bytes;
}

void UsbDeviceNode::noteProxyFail() { proxy_fails_++; }

bool UsbDeviceNode::copyLocalDeviceDescriptor(uint8_t *out18) const {
  if (!out18) {
    return false;
  }
  memcpy(out18, &g_dev_desc, 18);
  return true;
}

bool UsbDeviceNode::localStringAscii(uint8_t index, char *out,
                                     size_t out_len) const {
  if (!out || out_len == 0) {
    return false;
  }
  const char *src = nullptr;
  if (index == 1) {
    src = g_str_mfr;
  } else if (index == 2) {
    src = g_str_product;
  } else if (index == 3) {
    src = g_str_serial;
  } else {
    out[0] = '\0';
    return false;
  }
  strncpy(out, src, out_len - 1);
  out[out_len - 1] = '\0';
  return true;
}

bool UsbDeviceNode::selftestProxyRead(uint32_t lba, uint32_t blocks, uint8_t *out,
                                      String *err) {
  if (!proxy_on_ || !proxy_plane_) {
    if (err) {
      *err = "proxy_off";
    }
    return false;
  }
  if (!out || blocks == 0 || blocks > 16) {
    if (err) {
      *err = "bad_args";
    }
    return false;
  }
  if (!fillFromProxy(lba, 0, out, blocks * 512u)) {
    noteProxyFail();
    if (err) {
      *err = "proxy_read_fail";
    }
    return false;
  }
  noteProxyHit(blocks * 512u);
  return true;
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
      fillLabSector(slot->data, 512, cur_lba, 0);
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

void UsbDeviceNode::scheduleSoftReenum() {
  pending_reenum_ = true;
  reenum_at_ms_ = millis() + 80;
}

void UsbDeviceNode::runSoftReenum() {
  Serial.printf("[device] soft re-enum as %04x:%04x (%s)\n", g_dev_desc.idVendor,
                g_dev_desc.idProduct, g_str_product);
  g_msc.mediaPresent(false);
  tud_disconnect();
  delay(40);
  g_msc.mediaPresent(true);
  tud_connect();
  CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo, 0, 0,
                              g_dev_desc.idVendor, g_dev_desc.idProduct, 0,
                              "gadget_reident");
}

void UsbDeviceNode::applyLabIdentity() {
  g_dev_desc.idVendor = USB_ESPRESSIF_VID;
  g_dev_desc.idProduct = 0x1001;
  copyField(g_str_mfr, sizeof(g_str_mfr), "MPB", "MPB");
  copyField(g_str_product, sizeof(g_str_product), "ANALYZER", "ANALYZER");
  copyField(g_str_serial, sizeof(g_str_serial), "lab-gadget", "lab-gadget");
  setMscInquiry("MPB", "ANALYZER", "0001");
  gadget_blocks_ = 512;
  gadget_block_size_ = 512;
  g_msc.begin(gadget_blocks_, gadget_block_size_);
  applied_vid_ = g_dev_desc.idVendor;
  applied_pid_ = g_dev_desc.idProduct;
  identity_applied_ = false;
}

void UsbDeviceNode::setAoaRespond(bool on, bool reenum_on_start) {
  AoaLabResponder &aoa = AoaLabResponder::instance();
  aoa.bind(this);
  aoa.setReenumOnStart(reenum_on_start);
  aoa.setEnabled(on);
  if (!on) {
    // Restore lab MSC identity if we previously switched to accessory VID.
    if (applied_vid_ == 0x18D1) {
      applyLabIdentity();
      scheduleSoftReenum();
    }
  }
}

bool UsbDeviceNode::aoaRespondEnabled() const {
  return AoaLabResponder::instance().enabled();
}

void UsbDeviceNode::applyAoaAccessoryIdentity() {
  // AOSP after START: Google VID + accessory PID. Lab keeps MSC IF —
  // claim is control-handshake + VID switch only, not accessory bulk.
  g_dev_desc.idVendor = 0x18D1;
  g_dev_desc.idProduct = 0x2D00;
  copyField(g_str_mfr, sizeof(g_str_mfr), "Google", "Google");
  copyField(g_str_product, sizeof(g_str_product), "AOA Lab", "AOA Lab");
  copyField(g_str_serial, sizeof(g_str_serial), "aoa-lab", "aoa-lab");
  setMscInquiry("Google", "AOA Lab", "0001");
  applied_vid_ = g_dev_desc.idVendor;
  applied_pid_ = g_dev_desc.idProduct;
  identity_applied_ = true;
  Serial.printf("[device] AOA accessory identity %04x:%04x (MSC IF remains)\n",
                applied_vid_, applied_pid_);
  scheduleSoftReenum();
}

bool UsbDeviceNode::applyDutIdentity(const DutInfoSnapshot &snap) {
  if (!snap.present || snap.vid == 0) {
    return false;
  }

  const bool first_apply = !identity_applied_;
  const bool usb_changed =
      first_apply || (applied_vid_ != snap.vid) || (applied_pid_ != snap.pid);

  g_dev_desc.idVendor = snap.vid;
  g_dev_desc.idProduct = snap.pid;
  copyField(g_str_mfr, sizeof(g_str_mfr), snap.mfr, "DUT");
  copyField(g_str_product, sizeof(g_str_product), snap.product, "proxied");
  copyField(g_str_serial, sizeof(g_str_serial), snap.serial, "proxy");

  const char *inq_v = (snap.inq_vendor[0]) ? snap.inq_vendor : g_str_mfr;
  const char *inq_p = (snap.inq_product[0]) ? snap.inq_product : g_str_product;
  const char *inq_r = (snap.inq_rev[0]) ? snap.inq_rev : "0001";
  setMscInquiry(inq_v, inq_p, inq_r);

  uint16_t bsz = snap.block_size ? snap.block_size : 512;
  // Proxy path assumes 512-byte sectors for now.
  if (bsz != 512) {
    Serial.printf("[device] DUT block_size=%u — gadget stays 512 (proxy limit)\n",
                  bsz);
    bsz = 512;
  }
  uint32_t blocks = snap.capacity_blocks;
  if (blocks == 0) {
    blocks = gadget_blocks_ ? gadget_blocks_ : 512;
  }
  // Cap absurd sizes so TinyUSB RAM path stays sane in lab.
  if (blocks > 64u * 1024u * 1024u) {
    blocks = 64u * 1024u * 1024u;
  }
  const bool capacity_changed =
      (gadget_blocks_ != blocks) || (gadget_block_size_ != bsz);
  gadget_block_size_ = bsz;
  gadget_blocks_ = blocks;
  g_msc.begin(gadget_blocks_, gadget_block_size_);

  applied_vid_ = snap.vid;
  applied_pid_ = snap.pid;
  identity_applied_ = true;

  Serial.printf("[device] apply DUT identity %04x:%04x '%s' blocks=%u msc=%u\n",
                snap.vid, snap.pid, g_str_product,
                static_cast<unsigned>(gadget_blocks_), snap.msc_ready);

  if (usb_changed) {
    scheduleSoftReenum();
  } else if (capacity_changed) {
    // Capacity/inquiry-only change: unit attention via media toggle.
    g_msc.mediaPresent(false);
    delay(20);
    g_msc.mediaPresent(true);
  }
  return true;
}

void UsbDeviceNode::setProxy(DataPlane *plane, const String &peer_ip) {
  proxy_plane_ = plane;
  proxy_peer_ = peer_ip;
  proxy_on_ = plane && peer_ip.length() > 0;
  memset(&proxied_dut_, 0, sizeof(proxied_dut_));
  if (!proxy_on_) {
    if (plane) {
      plane->closeProxySession();
    }
    applyLabIdentity();
    scheduleSoftReenum();
  }
  Serial.printf("[device] MSC proxy %s peer=%s\n", proxy_on_ ? "ON" : "OFF",
                proxy_peer_.c_str());
}

bool UsbDeviceNode::refreshProxiedDut() {
  if (!proxy_on_ || !proxy_plane_) {
    return false;
  }
  String err;
  DutInfoSnapshot snap{};
  if (!proxy_plane_->proxyDutInfo(proxy_peer_, snap, &err)) {
    Serial.printf("[device] dut-info proxy fail: %s\n", err.c_str());
    return false;
  }
  proxied_dut_ = snap;
  CaptureLog::instance().push(CapRole::Device, CapType::EnumInfo, 0, snap.address,
                              snap.vid, snap.pid, 0, "proxied_dut");
  Serial.printf("[device] proxied DUT %04x:%04x %s msc=%u cap=%u\n", snap.vid,
                snap.pid, snap.product, snap.msc_ready,
                static_cast<unsigned>(snap.capacity_blocks));
  applyDutIdentity(snap);
  return true;
}

bool UsbDeviceNode::fillFromProxy(uint32_t lba, uint32_t offset, uint8_t *out,
                                  uint32_t bufsize) {
  if (!proxy_on_ || !proxy_plane_ || !out || bufsize == 0) {
    return false;
  }
  if (offset != 0 || (bufsize % 512u) != 0) {
    return false;
  }
  uint32_t blocks = bufsize / 512u;
  uint32_t done = 0;
  while (done < blocks) {
    uint32_t n = blocks - done;
    if (n > 16) {
      n = 16;
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

bool UsbDeviceNode::writeThroughProxy(uint32_t lba, uint32_t offset,
                                      const uint8_t *in, uint32_t bufsize) {
  if (!proxy_on_ || !proxy_plane_ || !in || bufsize == 0) {
    return false;
  }
  if (offset != 0 || (bufsize % 512u) != 0) {
    return false;
  }
  uint32_t blocks = bufsize / 512u;
  uint32_t done = 0;
  while (done < blocks) {
    uint32_t n = blocks - done;
    if (n > 16) {
      n = 16;
    }
    String err;
    if (!proxy_plane_->proxyMscWrite(proxy_peer_, lba + done, n,
                                     in + done * 512u, n * 512u, &err)) {
      Serial.printf("[device] proxy write fail lba=%u n=%u err=%s\n", lba + done,
                    n, err.c_str());
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
  json += ",\"identityApplied\":";
  json += identity_applied_ ? "true" : "false";
  json += ",\"appliedId\":\"";
  {
    char id[12];
    snprintf(id, sizeof(id), "%04x:%04x", applied_vid_, applied_pid_);
    json += id;
  }
  json += "\",\"blockCount\":";
  json += String(gadget_blocks_);
  json += ",\"blockSize\":";
  json += String(gadget_block_size_);
  json += ",\"proxyHits\":";
  json += String(proxy_hits_);
  json += ",\"proxyFails\":";
  json += String(proxy_fails_);
  json += ",\"proxyBytes\":";
  json += String(proxy_bytes_);
  json += ",\"claim\":\"gadget_identity_from_dut_snapshot\"";
  AoaLabResponder::instance().appendStatusJson(json);
  json += ",\"proxiedDut\":{";
  json += "\"present\":";
  json += proxied_dut_.present ? "true" : "false";
  if (proxied_dut_.present) {
    char id[12];
    snprintf(id, sizeof(id), "%04x:%04x", proxied_dut_.vid, proxied_dut_.pid);
    json += ",\"id\":\"";
    json += id;
    json += "\",\"vid\":";
    json += String(proxied_dut_.vid);
    json += ",\"pid\":";
    json += String(proxied_dut_.pid);
    json += ",\"product\":\"";
    json += proxied_dut_.product;
    json += "\",\"mscReady\":";
    json += proxied_dut_.msc_ready ? "true" : "false";
    json += ",\"capacityBlocks\":";
    json += String(proxied_dut_.capacity_blocks);
    json += ",\"blockSize\":";
    json += String(proxied_dut_.block_size);
    json += ",\"note\":\"descriptor_snapshot_not_usb_control_mitm\"";
  }
  json += "}";
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

  applyLabIdentity();
  AoaLabResponder::instance().bind(this);
  // Also seed Arduino USB.* before begin (used if weak override fails to link).
  USB.VID(g_dev_desc.idVendor);
  USB.PID(g_dev_desc.idProduct);
  USB.manufacturerName(g_str_mfr);
  USB.productName(g_str_product);
  USB.serialNumber(g_str_serial);

  USB.onEvent(onEvent);
  g_msc.onRead(onRead);
  g_msc.onWrite(onWrite);
  g_msc.onStartStop(onStartStop);
  g_msc.mediaPresent(true);
  g_msc.isWritable(true);
  g_msc.begin(gadget_blocks_, gadget_block_size_);

  USB.begin();
  begin_ok_ = true;
  ready_ = true;
  mounted_ = static_cast<bool>(USB);
  Serial.println("[device] USB.begin() issued — wait for host enumeration");
  Serial.printf("[device] otg_state=%s id=%04x:%04x\n", otgState(),
                g_dev_desc.idVendor, g_dev_desc.idProduct);
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
  if (AoaLabResponder::instance().consumeStartPending()) {
    applyAoaAccessoryIdentity();
  }
  if (pending_reenum_ && static_cast<int32_t>(millis() - reenum_at_ms_) >= 0) {
    pending_reenum_ = false;
    runSoftReenum();
  }
  if (proxy_on_ && proxy_plane_ && !proxied_dut_.present) {
    static uint32_t last_try_ms = 0;
    const uint32_t t = millis();
    if (t - last_try_ms >= 2000) {
      last_try_ms = t;
      refreshProxiedDut();
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
void UsbDeviceNode::noteProxyHit(uint32_t) {}
void UsbDeviceNode::noteProxyFail() {}
void UsbDeviceNode::setProxy(DataPlane *, const String &) {}
bool UsbDeviceNode::refreshProxiedDut() { return false; }
bool UsbDeviceNode::selftestProxyRead(uint32_t, uint32_t, uint8_t *, String *) {
  return false;
}
bool UsbDeviceNode::copyLocalDeviceDescriptor(uint8_t *) const { return false; }
bool UsbDeviceNode::localStringAscii(uint8_t, char *, size_t) const {
  return false;
}
void UsbDeviceNode::setAoaRespond(bool, bool) {}
bool UsbDeviceNode::aoaRespondEnabled() const { return false; }
void UsbDeviceNode::applyAoaAccessoryIdentity() {}
bool UsbDeviceNode::writeThroughProxy(uint32_t, uint32_t, const uint8_t *,
                                      uint32_t) {
  return false;
}
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
void UsbDeviceNode::applyLabIdentity() {}
bool UsbDeviceNode::applyDutIdentity(const DutInfoSnapshot &) { return false; }
void UsbDeviceNode::scheduleSoftReenum() {}
void UsbDeviceNode::runSoftReenum() {}

} // namespace usb_analyzer

#endif
