#include "l0/GpioBitbangTiming.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace usb_analyzer {

bool GpioBitbangTiming::calibrated_ = false;
uint32_t GpioBitbangTiming::cycles_per_us_ = 240; // ESP32-S3 default guess

GpioBitbangTiming::CalResult GpioBitbangTiming::calibrate(uint32_t spin_us) {
  CalResult r;
  r.cpu_mhz = getCpuFrequencyMhz();
  if (spin_us < 100) {
    spin_us = 100;
  }
  if (spin_us > 50000) {
    spin_us = 50000;
  }
  portDISABLE_INTERRUPTS();
  const uint32_t c0 = esp_cpu_get_cycle_count();
  const int64_t t0 = esp_timer_get_time();
  // Fixed cycle burn: 1000 * cpu_mhz cycles ≈ 1000 us if mhz accurate
  const uint32_t target = spin_us * (r.cpu_mhz ? r.cpu_mhz : 240);
  while ((esp_cpu_get_cycle_count() - c0) < target) {
  }
  const int64_t t1 = esp_timer_get_time();
  const uint32_t c1 = esp_cpu_get_cycle_count();
  portENABLE_INTERRUPTS();

  r.measure_us = static_cast<uint32_t>(t1 - t0);
  r.measured_cycles = c1 - c0;
  if (r.measure_us > 0) {
    r.cycles_per_us = r.measured_cycles / r.measure_us;
  } else {
    r.cycles_per_us = r.cpu_mhz ? r.cpu_mhz : 240;
  }
  const uint32_t expect = spin_us;
  const uint32_t delta =
      r.measure_us > expect ? r.measure_us - expect : expect - r.measure_us;
  r.err_ppm = expect ? static_cast<uint32_t>((delta * 1000000ULL) / expect) : 0;
  cycles_per_us_ = r.cycles_per_us ? r.cycles_per_us : 240;
  calibrated_ = true;
  r.ok = true;
  r.note = "cycle_count_vs_esp_timer";
  return r;
}

void GpioBitbangTiming::waitUs(uint32_t us) {
  if (us == 0) {
    return;
  }
  if (!calibrated_) {
    const int64_t t1 = esp_timer_get_time() + us;
    while (esp_timer_get_time() < t1) {
    }
    return;
  }
  const uint32_t need = us * cycles_per_us_;
  const uint32_t c0 = esp_cpu_get_cycle_count();
  while ((esp_cpu_get_cycle_count() - c0) < need) {
  }
}

} // namespace usb_analyzer
