#include "device/UsbDeviceNode.h"

#if ANALYZER_ENABLE_USB_DEVICE

#include "USB.h"
#include "USBMSC.h"

namespace usb_analyzer {
namespace {

USBMSC g_msc;
static uint8_t g_sector[512];

int32_t onRead(uint32_t /*lba*/, uint32_t offset, void *buffer, uint32_t bufsize) {
  // Minimal zero-fill gadget — App callback ≠ BOT wire event (REJECTED equality).
  uint8_t *out = static_cast<uint8_t *>(buffer);
  for (uint32_t i = 0; i < bufsize; i++) {
    out[i] = 0;
  }
  (void)offset;
  return static_cast<int32_t>(bufsize);
}

bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  Serial.printf("[device] start_stop power=%u start=%d eject=%d\n", power_condition,
                start ? 1 : 0, load_eject ? 1 : 0);
  return true;
}

void onEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;
  (void)event_base;
  (void)event_data;
  if (event_id == ARDUINO_USB_STARTED_EVENT) {
    Serial.println("[device] USB STARTED (bus activity)");
  }
  if (event_id == ARDUINO_USB_STOPPED_EVENT) {
    Serial.println("[device] USB STOPPED");
  }
  if (event_id == ARDUINO_USB_SUSPEND_EVENT) {
    Serial.println("[device] USB SUSPEND");
  }
  if (event_id == ARDUINO_USB_RESUME_EVENT) {
    Serial.println("[device] USB RESUME");
  }
}

} // namespace

bool UsbDeviceNode::begin() {
  Serial.println("[device] TinyUSB MSC gadget bring-up (Gate 2)");
  Serial.println("[device] CLAIM: gadget present to host — not wire capture, not MITM");
  Serial.println("[device] Debug: UART/COM only; OTG port is the gadget toward PC/HU");

  USB.onEvent(onEvent);
  g_msc.vendorID("MPB");
  g_msc.productID("ANALYZER");
  g_msc.productRevision("0001");
  g_msc.onRead(onRead);
  g_msc.onStartStop(onStartStop);
  // 256 KiB virtual disk (512 B × 512)
  g_msc.mediaPresent(true);
  g_msc.begin(512, 512);

  USB.begin();
  begin_ok_ = true;
  ready_ = true;
  Serial.println("[device] USB.begin() issued — wait for host enumeration");
  Serial.println("[device] NOTE: SET_ADDRESS ≠ mounted; mount at SET_CONFIGURATION");
  (void)g_sector;
  return begin_ok_;
}

void UsbDeviceNode::loop() {
  // TinyUSB runs in its own task under Arduino-ESP32.
}

} // namespace usb_analyzer

#else // !ANALYZER_ENABLE_USB_DEVICE

namespace usb_analyzer {

bool UsbDeviceNode::begin() {
  Serial.println("[device] ANALYZER_ENABLE_USB_DEVICE=0 — stub");
  return false;
}
void UsbDeviceNode::loop() {}

} // namespace usb_analyzer

#endif
