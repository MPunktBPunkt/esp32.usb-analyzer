#pragma once

/**
 * User-editable defaults. No WiFi secrets in git — use WiFiManager portal.
 */

#ifndef DEVICE_NAME_PREFIX
#define DEVICE_NAME_PREFIX "usb-analyzer"
#endif

#ifndef FW_VERSION
#define FW_VERSION "0.1.0"
#endif

#ifndef WIFI_AP_NAME
#define WIFI_AP_NAME "USB-Analyzer-Setup"
#endif

#ifndef WIFI_PORTAL_TIMEOUT_S
#define WIFI_PORTAL_TIMEOUT_S 180
#endif

/** Debian control plane — override in portal / NVS. */
#ifndef CONTROLLER_HOST_DEFAULT
#define CONTROLLER_HOST_DEFAULT "192.168.178.113"
#endif

#ifndef CONTROLLER_PORT_DEFAULT
#define CONTROLLER_PORT_DEFAULT 9750
#endif

#ifndef HELLO_INTERVAL_MS
#define HELLO_INTERVAL_MS 5000
#endif

#ifndef RESET_BUTTON_PIN
#define RESET_BUTTON_PIN 0
#endif

#ifndef RESET_HOLD_SEC
#define RESET_HOLD_SEC 3
#endif

#ifndef GIT_SHA
#define GIT_SHA "unknown"
#endif
