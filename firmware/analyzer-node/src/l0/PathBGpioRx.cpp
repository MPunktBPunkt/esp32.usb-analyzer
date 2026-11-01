#include "l0/PathBGpioRx.h"
#include "l0/GpioBitbangTiming.h"
#include "l0/GpioSyncMarker.h"

#include <WiFi.h>
#include <cstring>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <vector>

namespace usb_analyzer {
namespace {

uint8_t readState(gpio_num_t dp, gpio_num_t dm) {
  const int dpp = gpio_get_level(dp);
  const int dmm = gpio_get_level(dm);
  if (!dpp && !dmm) {
    return 0;
  }
  if (dpp && !dmm) {
    return 1;
  }
  if (!dpp && dmm) {
    return 2;
  }
  return 3;
}

bool pushL0st(const String &host, uint16_t port, uint32_t rate_hz,
              const uint8_t *states, uint32_t n, uint32_t *out_bytes,
              String *err) {
  WiFiClient c;
  if (!c.connect(host.c_str(), port, 3000)) {
    if (err) {
      *err = "l0st_connect_failed";
    }
    return false;
  }
  auto send_frame = [&](uint16_t typ, uint32_t seq, const uint8_t *p,
                        uint32_t nbytes) -> bool {
    uint8_t hdr[20];
    const uint32_t magic = 0x4C305354u;
    memcpy(hdr + 0, &magic, 4);
    uint16_t ver = 1;
    memcpy(hdr + 4, &ver, 2);
    memcpy(hdr + 6, &typ, 2);
    memcpy(hdr + 8, &rate_hz, 4);
    memcpy(hdr + 12, &seq, 4);
    memcpy(hdr + 16, &nbytes, 4);
    if (c.write(hdr, sizeof(hdr)) != sizeof(hdr)) {
      return false;
    }
    if (nbytes && c.write(p, nbytes) != nbytes) {
      return false;
    }
    if (out_bytes) {
      *out_bytes += sizeof(hdr) + nbytes;
    }
    return true;
  };
  const char *hello =
      "{\"sampler\":\"path_b_gpio_rx\",\"note\":\"crosslink_or_local\"}";
  if (!send_frame(3, 0, reinterpret_cast<const uint8_t *>(hello),
                  strlen(hello)) ||
      !send_frame(1, 1, states, n) || !send_frame(2, 1, nullptr, 0)) {
    if (err) {
      *err = "l0st_send_failed";
    }
    c.stop();
    return false;
  }
  c.flush();
  c.stop();
  return true;
}

} // namespace

PathBGpioRx::Result PathBGpioRx::run(const String &mode, uint32_t samples,
                                     uint32_t rate_hz, int dp_pin, int dm_pin,
                                     int sync_pin, uint32_t timeout_ms,
                                     const String &l0st_host,
                                     uint16_t l0st_port) {
  Result r;
  r.mode = mode.length() ? mode : "rx";
  r.dp_pin = dp_pin > 0 ? dp_pin : kDefaultDpPin;
  r.dm_pin = dm_pin > 0 ? dm_pin : kDefaultDmPin;
  r.sync_pin = sync_pin > 0 ? sync_pin : kDefaultSyncPin;
  r.rate_hz = rate_hz ? rate_hz : 100000;
  r.note = "gpio_rx_free_pins_not_otg_phy";

  if (r.dp_pin == 19 || r.dp_pin == 20 || r.dm_pin == 19 || r.dm_pin == 20 ||
      r.sync_pin == 19 || r.sync_pin == 20) {
    r.error = "refuse_gpio19_20";
    return r;
  }
  if (samples < 8) {
    samples = 8;
  }
  if (samples > kMaxSamples) {
    samples = kMaxSamples;
  }
  if (r.rate_hz > 2000000) {
    r.rate_hz = 2000000;
  }
  if (!timeout_ms) {
    timeout_ms = 3000;
  }

  const bool want_sync =
      r.mode == "rx_sync" || r.mode == "rx_sync_l0st";
  const bool want_l0st =
      r.mode == "rx_l0st" || r.mode == "rx_sync_l0st";

  if (want_sync) {
    r.sync_seen = GpioSyncMarker::waitEdge(r.sync_pin, timeout_ms, true);
    if (!r.sync_seen) {
      r.sync_timeout = true;
      r.error = "sync_timeout";
      // Still sample — documents floating/no-jumper case.
    }
  }

  gpio_config_t io = {};
  io.pin_bit_mask = (1ULL << r.dp_pin) | (1ULL << r.dm_pin);
  io.mode = GPIO_MODE_INPUT;
  io.pull_up_en = GPIO_PULLUP_DISABLE;
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&io) != ESP_OK) {
    r.error = "gpio_config_failed";
    return r;
  }

  std::vector<uint8_t> buf(samples);
  const gpio_num_t dp = static_cast<gpio_num_t>(r.dp_pin);
  const gpio_num_t dm = static_cast<gpio_num_t>(r.dm_pin);
  const uint32_t period_us = r.rate_hz ? (1000000UL / r.rate_hz) : 10;
  uint8_t prev = readState(dp, dm);
  const int64_t t0 = esp_timer_get_time();
  for (uint32_t i = 0; i < samples; i++) {
    const uint8_t st = readState(dp, dm);
    buf[i] = st;
    r.hist[st & 3]++;
    if (i && st != prev) {
      r.edges++;
    }
    prev = st;
    GpioBitbangTiming::waitUs(period_us ? period_us : 1);
  }
  const int64_t t1 = esp_timer_get_time();
  r.samples = samples;
  r.duration_us = static_cast<uint32_t>(t1 - t0);
  if (r.duration_us > 0) {
    r.achieved_rate_hz = static_cast<uint32_t>(
        (static_cast<uint64_t>(samples) * 1000000ULL) / r.duration_us);
  }

  if (want_l0st) {
    if (!l0st_host.length()) {
      r.error = "l0st_host_required";
    } else {
      String err;
      r.l0st_ok =
          pushL0st(l0st_host, l0st_port ? l0st_port : kDefaultL0stPort,
                   r.achieved_rate_hz ? r.achieved_rate_hz : r.rate_hz,
                   buf.data(), samples, &r.l0st_bytes, &err);
      if (!r.l0st_ok) {
        r.error = err;
      }
    }
  }

  // ok if we sampled; sync_timeout alone is informational when jumper absent
  r.ok = (r.samples > 0) && (!want_l0st || r.l0st_ok);
  if (r.sync_timeout && !r.error.length()) {
    r.note += ";sync_timeout_no_jumper_or_late_tx";
  }
  return r;
}

} // namespace usb_analyzer
