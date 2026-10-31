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

#ifdef __cplusplus
}
#endif
