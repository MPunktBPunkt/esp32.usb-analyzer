#include "l0/WireStimulus.h"
#include "l0/GpioBitbangTiming.h"
#include "l0/GpioLinePatterns.h"

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

} // namespace

WireStimulus::Result WireStimulus::run(const String &pattern, uint32_t rate_hz,
                                       uint32_t reboot_after_ms,
                                       uint32_t post_idle_ms) {
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
  if (!GpioBitbangTiming::calibrated()) {
    GpioBitbangTiming::calibrate(1000);
  }

  // Map legacy setup_ok → wire NRZI table
  String pat = r.pattern;
  if (pat == "setup_ok") {
    pat = "setup_ok_wire";
  }
  std::vector<uint8_t> states = buildGpioLinePattern(pat, &err);
  if (states.empty()) {
    r.error = err.length() ? err : "empty_pattern";
    return r;
  }

  const uint32_t period_us = r.rate_hz ? (1000000UL / r.rate_hz) : 10;
  const int64_t t0 = esp_timer_get_time();
  portDISABLE_INTERRUPTS();
  for (uint8_t st : states) {
    drive(st);
    GpioBitbangTiming::waitUs(period_us > 0 ? period_us : 1);
  }
  drive(1); // idle J
  portENABLE_INTERRUPTS();
  const int64_t t_bit = esp_timer_get_time();
  r.active_us = static_cast<uint32_t>(t_bit - t0);
  if (post_idle_ms > 0) {
    delay(post_idle_ms);
  }
  const int64_t t1 = esp_timer_get_time();

  r.states = static_cast<uint32_t>(states.size());
  r.duration_us = static_cast<uint32_t>(t1 - t0);
  if (r.active_us > 0) {
    r.achieved_rate_hz = static_cast<uint32_t>(
        (static_cast<uint64_t>(r.states) * 1000000ULL) / r.active_us);
  }
  r.ok = true;
  return r;
}

WireStimulus::Result WireStimulus::runSeries(const String &patterns_csv,
                                             uint32_t rate_hz,
                                             uint32_t reboot_after_ms,
                                             uint32_t gap_ms) {
  Result agg;
  agg.rate_hz = rate_hz ? rate_hz : 100000;
  agg.reboot_after_ms = reboot_after_ms;
  agg.host_released = true;
  agg.pattern = patterns_csv;
  agg.note = "series;destructive_otg_gpio19_20_stimulus";

  String csv = patterns_csv;
  if (!csv.length()) {
    csv = "bus_states";
  }
  uint32_t total_states = 0;
  uint32_t total_us = 0;
  uint32_t total_active = 0;
  uint32_t n_ok = 0;
  int start = 0;
  while (start <= static_cast<int>(csv.length())) {
    int comma = csv.indexOf(',', start);
    String one = (comma < 0) ? csv.substring(start) : csv.substring(start, comma);
    one.trim();
    if (one.length()) {
      Result r = run(one, rate_hz, 0, gap_ms);
      if (!r.ok) {
        agg.error = r.error;
        agg.pattern = one;
        return agg;
      }
      total_states += r.states;
      total_us += r.duration_us;
      total_active += r.active_us;
      n_ok++;
      agg.pins_claimed = r.pins_claimed;
    }
    if (comma < 0) {
      break;
    }
    start = comma + 1;
  }
  agg.states = total_states;
  agg.duration_us = total_us;
  agg.active_us = total_active;
  if (total_active > 0) {
    agg.achieved_rate_hz = static_cast<uint32_t>(
        (static_cast<uint64_t>(total_states) * 1000000ULL) / total_active);
  }
  agg.ok = n_ok > 0;
  agg.pattern = String("series:") + String(n_ok) + ":" + patterns_csv;
  return agg;
}

} // namespace usb_analyzer
