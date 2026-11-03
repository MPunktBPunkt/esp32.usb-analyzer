#pragma once

#include "ProjectConfig.h"
#include <stddef.h>
#include <stdint.h>

#if ANALYZER_ENABLE_USB_HOST
class EspUsbHost;

namespace usb_analyzer {

/** MODE_SENSE(6) via EspUsbHost private mscCommand (lab unlock). */
bool mscLabModeSense6(EspUsbHost &usb, uint8_t page_code = 0x3F,
                      uint8_t alloc_len = 192, uint8_t *out = nullptr,
                      size_t out_len = 0, uint32_t timeout_ms = 2000);

/** PREVENT/ALLOW MEDIUM REMOVAL (SCSI 0x1E). */
bool mscLabPreventAllow(EspUsbHost &usb, bool prevent = true,
                        uint32_t timeout_ms = 2000);

} // namespace usb_analyzer
#endif
