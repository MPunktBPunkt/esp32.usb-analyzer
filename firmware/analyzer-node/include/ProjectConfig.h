#pragma once

/**
 * User-editable defaults. No WiFi secrets in git — use WiFiManager portal.
 * Includes esp-hub-base ProjectConfig macros (Stufe A).
 */

#ifndef DEVICE_NAME_PREFIX
#define DEVICE_NAME_PREFIX "usb-analyzer"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "usb-analyzer"
#endif

#ifndef FW_VERSION
#define FW_VERSION "0.0.3"
#endif

#ifndef ESPHUB_ENABLE_EXAMPLE_MODULE
#define ESPHUB_ENABLE_EXAMPLE_MODULE 0
#endif

/** ioBroker esp-hub (FW distributor). */
#ifndef HUB_HOST
#define HUB_HOST "192.168.178.113"
#endif

#ifndef HUB_PORT
#define HUB_PORT 8093
#endif

#ifndef WIFI_AP_NAME
#define WIFI_AP_NAME "USB-Analyzer-Setup"
#endif

#ifndef WIFI_PORTAL_TIMEOUT_S
#define WIFI_PORTAL_TIMEOUT_S 180
#endif

#ifndef DEFAULT_INTERVAL_S
#define DEFAULT_INTERVAL_S 30
#endif

#ifndef WATCHDOG_TIMEOUT_S
#define WATCHDOG_TIMEOUT_S 300
#endif

#ifndef PROGRESS_TIMEOUT_S
#define PROGRESS_TIMEOUT_S WATCHDOG_TIMEOUT_S
#endif

/** Debian control plane — override in portal / NVS. */
#ifndef CONTROLLER_HOST_DEFAULT
#define CONTROLLER_HOST_DEFAULT "192.168.178.187"
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

/**
 * Onboard WS2812 RGB (YD-ESP32-S3 / DevKitC-1 class: GPIO48).
 * Some YD boards need the RGB solder bridge closed for the LED to light.
 */
#ifndef RGB_LED_PIN
#define RGB_LED_PIN 48
#endif

/* GIT_SHA comes from include/build_id.h (generated pre-build). */
