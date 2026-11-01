#include "l0/GpioSyncMarker.h"
#include "l0/GpioBitbangTiming.h"

#include <esp_timer.h>

namespace usb_analyzer {

void GpioSyncMarker::pulseOut(int pin, uint32_t high_us) {
  if (pin == 19 || pin == 20) {
    return;
  }
  gpio_reset_pin(static_cast<gpio_num_t>(pin));
  gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
  gpio_set_level(static_cast<gpio_num_t>(pin), 0);
  GpioBitbangTiming::waitUs(10);
  gpio_set_level(static_cast<gpio_num_t>(pin), 1);
  GpioBitbangTiming::waitUs(high_us ? high_us : 50);
  gpio_set_level(static_cast<gpio_num_t>(pin), 0);
}

bool GpioSyncMarker::waitEdge(int pin, uint32_t timeout_ms, bool rising) {
  if (pin == 19 || pin == 20) {
    return false;
  }
  gpio_reset_pin(static_cast<gpio_num_t>(pin));
  gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_INPUT);
  gpio_set_pull_mode(static_cast<gpio_num_t>(pin), GPIO_PULLDOWN_ONLY);
  const int want = rising ? 1 : 0;
  const int64_t t_end = esp_timer_get_time() +
                        static_cast<int64_t>(timeout_ms) * 1000;
  int prev = gpio_get_level(static_cast<gpio_num_t>(pin));
  while (esp_timer_get_time() < t_end) {
    const int now = gpio_get_level(static_cast<gpio_num_t>(pin));
    if (prev != now && now == want) {
      return true;
    }
    prev = now;
  }
  return false;
}

} // namespace usb_analyzer
