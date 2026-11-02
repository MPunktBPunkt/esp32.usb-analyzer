#pragma once

/**
 * Gate-6: hooks called from vendored TinyUSB msc_device.c (0.18.0).
 * Captures real BOT CBW/CSW bytes — not App-callback fiction, not D+/D− wire.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void analyzer_msc_on_cbw(const void *cbw, uint32_t len);
void analyzer_msc_on_csw(const void *csw, uint32_t len);
/** CLEAR_FEATURE(ENDPOINT_HALT) seen in mscd_control_xfer — not BOT_RESET. */
void analyzer_msc_on_clear_halt(uint8_t ep_addr, uint8_t need_reset_stage);
/** MSC class BOT Mass Storage Reset (bRequest 0xFF). */
void analyzer_msc_on_bot_reset(void);

#ifdef __cplusplus
}
#endif
