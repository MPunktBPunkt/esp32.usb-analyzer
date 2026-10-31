/**
 * TinyUSB weak complete hooks — one step above App onRead/onWrite.
 * Still NOT wire CBW bytes (those live inside prebuilt msc_device.c.obj).
 * source: class_complete_cb
 */

#include "ProjectConfig.h"

#if ANALYZER_ENABLE_USB_DEVICE

#include "capture/CaptureLog.h"

#include <Arduino.h>
#include <cstring>

extern "C" {

void tud_msc_read10_complete_cb(uint8_t lun) {
  (void)lun;
  usb_analyzer::CaptureLog::instance().push(
      usb_analyzer::CapRole::Device, usb_analyzer::CapType::BotCsw, 0, 13, 0, 0,
      0, "READ10_complete");
}

void tud_msc_write10_complete_cb(uint8_t lun) {
  (void)lun;
  usb_analyzer::CaptureLog::instance().push(
      usb_analyzer::CapRole::Device, usb_analyzer::CapType::BotCsw, 0, 13, 0, 0,
      0, "WRITE10_complete");
}

void tud_msc_scsi_complete_cb(uint8_t lun, uint8_t const scsi_cmd[16]) {
  (void)lun;
  char note[24];
  snprintf(note, sizeof(note), "SCSI_0x%02X_done", scsi_cmd ? scsi_cmd[0] : 0);
  usb_analyzer::CaptureLog::instance().push(
      usb_analyzer::CapRole::Device, usb_analyzer::CapType::BotCsw, 0, 13,
      scsi_cmd ? scsi_cmd[0] : 0, 0, 0, note);
}

} // extern "C"

#endif
