#include "l0/WireStimulus.h"
#include "l0/WireStimDataBadCrc.h"
#include "l0/WireStimSetupBadCrc.h"
#include "l0/WireStimSetupOk.h"
#include "l0/WireStimSync.h"

#include <cstring>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

namespace usb_analyzer {
namespace {

void drive(uint8_t st) {
  int dp = 0, dm = 0;
  switch (st & 3) {
  case 0:
    break; // SE0
  case 1:
    dp = 1;
    break; // J
  case 2:
    dm = 1;
    break; // K
  default:
    dp = 1;
    dm = 1;
    break; // SE1
  }
  gpio_set_level(GPIO_NUM_20, dp); // D+
  gpio_set_level(GPIO_NUM_19, dm); // D−
}

void waitUs(uint32_t us) {
  if (!us) {
    return;
  }
  const int64_t t1 = esp_timer_get_time() + us;
  while (esp_timer_get_time() < t1) {
  }
}

bool claimPins(String *err) {
  gpio_reset_pin(GPIO_NUM_19);
  gpio_reset_pin(GPIO_NUM_20);
  gpio_config_t io = {};
  io.pin_bit_mask = (1ULL << 19) | (1ULL << 20);
  io.mode = GPIO_MODE_OUTPUT;
  io.pull_up_en = GPIO_PULLUP_DISABLE;
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&io) != ESP_OK) {
    if (err) {
      *err = "gpio_config_failed";
    }
    return false;
  }
  drive(1); // idle J
  return true;
}

void appendHold(std::vector<uint8_t> &out, uint8_t st, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    out.push_back(st);
  }
}

std::vector<uint8_t> buildPattern(const String &pattern, String *err) {
  std::vector<uint8_t> out;
  if (pattern == "bus_states") {
    appendHold(out, 0, 64); // SE0
    appendHold(out, 1, 64); // J
    appendHold(out, 2, 64); // K
    appendHold(out, 3, 32); // SE1 illegal
    appendHold(out, 1, 64); // back to J
  } else if (pattern == "sync") {
    for (int rep = 0; rep < 16; rep++) {
      out.insert(out.end(), kWireStim_Sync,
                 kWireStim_Sync + kWireStim_Sync_Count);
      appendHold(out, 1, 8);
    }
  } else if (pattern == "eop") {
    appendHold(out, 1, 16);
    appendHold(out, 0, 2); // SE0 ×2
    appendHold(out, 1, 16);
  } else if (pattern == "se0_reset") {
    appendHold(out, 0, 200); // long SE0 ~ reset-ish at slow rates
    appendHold(out, 1, 64);
  } else if (pattern == "setup_ok") {
    out.assign(kWireStim_SetupOk, kWireStim_SetupOk + kWireStim_SetupOk_Count);
  } else if (pattern == "setup_bad_crc") {
    out.assign(kWireStim_SetupBadCrc,
               kWireStim_SetupBadCrc + kWireStim_SetupBadCrc_Count);
  } else if (pattern == "data_bad_crc") {
    out.assign(kWireStim_DataBadCrc,
               kWireStim_DataBadCrc + kWireStim_DataBadCrc_Count);
  } else if (pattern == "glitch") {
    appendHold(out, 1, 32);
    out.push_back(2);
    out.push_back(1); // 1-state K glitch
    appendHold(out, 1, 32);
    out.push_back(0);
    out.push_back(1); // 1-state SE0 glitch
    appendHold(out, 1, 32);
  } else if (pattern == "jk_burst") {
    for (int i = 0; i < 256; i++) {
      out.push_back((i & 1) ? 1 : 2);
    }
    appendHold(out, 1, 16);
  } else {
    if (err) {
      *err = "unknown_pattern";
    }
  }
  return out;
}

} // namespace

WireStimulus::Result WireStimulus::run(const String &pattern, uint32_t rate_hz,
                                       uint32_t reboot_after_ms) {
  Result r;
  r.pattern = pattern.length() ? pattern : "bus_states";
  r.rate_hz = rate_hz ? rate_hz : 100000;
  if (r.rate_hz > 2000000) {
    r.rate_hz = 2000000;
  }
  r.reboot_after_ms = reboot_after_ms;
  r.host_released = true; // caller must have stopped EspUsbHost first
  r.note = "destructive_otg_gpio19_20_stimulus;host_stack_stopped;"
           "reboot_required_to_restore_host";

  String err;
  if (!claimPins(&err)) {
    r.error = err;
    return r;
  }
  r.pins_claimed = true;

  std::vector<uint8_t> states = buildPattern(r.pattern, &err);
  if (states.empty()) {
    r.error = err.length() ? err : "empty_pattern";
    return r;
  }

  const uint32_t period_us = r.rate_hz ? (1000000UL / r.rate_hz) : 10;
  const int64_t t0 = esp_timer_get_time();
  // Critical section: reduce IRQ jitter for short patterns
  portDISABLE_INTERRUPTS();
  for (uint8_t st : states) {
    drive(st);
    waitUs(period_us > 0 ? period_us : 1);
  }
  drive(1); // idle J
  portENABLE_INTERRUPTS();
  const int64_t t1 = esp_timer_get_time();

  r.states = static_cast<uint32_t>(states.size());
  r.duration_us = static_cast<uint32_t>(t1 - t0);
  if (r.duration_us > 0) {
    r.achieved_rate_hz = static_cast<uint32_t>(
        (static_cast<uint64_t>(r.states) * 1000000ULL) / r.duration_us);
  }
  r.ok = true;

  if (r.reboot_after_ms > 0) {
    // Return HTTP response first — caller should schedule restart after send.
  }
  return r;
}

} // namespace usb_analyzer
