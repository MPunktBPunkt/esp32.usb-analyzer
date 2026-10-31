/**
 * TinyUSB weak complete hooks — App-adjacent stage markers.
 * Real CBW/CSW bytes come from MscClassHooks (class_hook_mscd_xfer).
 * These remain as optional stage notes (not BotCbw/BotCsw duplicates).
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
      usb_analyzer::CapRole::Device, usb_analyzer::CapType::MscRead, 0, 0, 0, 0,
      0, "READ10_complete");
}

void tud_msc_write10_complete_cb(uint8_t lun) {
  (void)lun;
  usb_analyzer::CaptureLog::instance().push(
      usb_analyzer::CapRole::Device, usb_analyzer::CapType::MscWrite, 0, 0, 0, 0,
      0, "WRITE10_complete");
}

void tud_msc_scsi_complete_cb(uint8_t lun, uint8_t const scsi_cmd[16]) {
  (void)lun;
  char note[24];
  snprintf(note, sizeof(note), "SCSI_0x%02X_done", scsi_cmd ? scsi_cmd[0] : 0);
  usb_analyzer::CaptureLog::instance().push(
      usb_analyzer::CapRole::Device, usb_analyzer::CapType::EnumInfo, 0, 0,
      scsi_cmd ? scsi_cmd[0] : 0, 0, 0, note);
}

} // extern "C"

#endif
