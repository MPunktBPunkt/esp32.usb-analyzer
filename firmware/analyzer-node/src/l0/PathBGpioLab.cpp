#include "l0/PathBGpioLab.h"
#include "l0/PathBGpioSynthStates.h"

#include <WiFi.h>
#include <cstring>
#include <driver/gpio.h>
#include <esp_timer.h>

namespace usb_analyzer {
namespace {

void applyState(gpio_num_t dp, gpio_num_t dm, uint8_t st) {
  // SE0=0 J=1 K=2 SE1=3  (FS: J idle = DP=1 DM=0)
  int dp_lvl = 0;
  int dm_lvl = 0;
  switch (st & 0x3) {
  case 0: // SE0
    dp_lvl = 0;
    dm_lvl = 0;
    break;
  case 1: // J
    dp_lvl = 1;
    dm_lvl = 0;
    break;
  case 2: // K
    dp_lvl = 0;
    dm_lvl = 1;
    break;
  default: // SE1
    dp_lvl = 1;
    dm_lvl = 1;
    break;
  }
  gpio_set_level(dp, dp_lvl);
  gpio_set_level(dm, dm_lvl);
}

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

void busyWaitUs(uint32_t us) {
  if (us == 0) {
    return;
  }
  const int64_t t1 = esp_timer_get_time() + static_cast<int64_t>(us);
  while (esp_timer_get_time() < t1) {
    // spin — intentional for bit-bang timing
  }
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
    // magic L0ST LE, ver=1, type, rate, seq, nbytes
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

  const char *hello = "{\"sampler\":\"path_b_gpio_lab\",\"note\":\"synth_txrx\"}";
  if (!send_frame(3, 0, reinterpret_cast<const uint8_t *>(hello),
                  strlen(hello))) {
    if (err) {
      *err = "l0st_hello_failed";
    }
    c.stop();
    return false;
  }
  if (!send_frame(1, 1, states, n)) {
    if (err) {
      *err = "l0st_chunk_failed";
    }
    c.stop();
    return false;
  }
  if (!send_frame(2, 1, nullptr, 0)) {
    if (err) {
      *err = "l0st_eob_failed";
    }
    c.stop();
    return false;
  }
  c.flush();
  c.stop();
  return true;
}

} // namespace

PathBGpioLab::Result PathBGpioLab::run(const String &mode, uint32_t rate_hz,
                                       int dp_pin, int dm_pin,
                                       const String &l0st_host,
                                       uint16_t l0st_port) {
  Result r;
  r.mode = mode.length() ? mode : "txrx";
  r.dp_pin = dp_pin > 0 ? dp_pin : kDefaultDpPin;
  r.dm_pin = dm_pin > 0 ? dm_pin : kDefaultDmPin;
  r.rate_hz = rate_hz == 0 ? 100000 : rate_hz; // default 100 kHz — slow lab
  r.l0st_host = l0st_host;
  r.l0st_port = l0st_port ? l0st_port : kDefaultL0stPort;
  r.note = "free_gpio_bitbang_not_otg_phy;default_100kHz";

  // Refuse OTG PHY pins.
  if (r.dp_pin == 19 || r.dp_pin == 20 || r.dm_pin == 19 || r.dm_pin == 20) {
    r.error = "refuse_gpio19_20_otg_phy";
    return r;
  }
  if (r.dp_pin == r.dm_pin) {
    r.error = "dp_dm_pins_must_differ";
    return r;
  }
  if (r.rate_hz > 2000000) {
    r.rate_hz = 2000000; // hard cap — not claiming FS 12 MHz on CPU bitbang
  }

  const gpio_num_t dp = static_cast<gpio_num_t>(r.dp_pin);
  const gpio_num_t dm = static_cast<gpio_num_t>(r.dm_pin);
  gpio_config_t io = {};
  io.pin_bit_mask = (1ULL << r.dp_pin) | (1ULL << r.dm_pin);
  io.mode = GPIO_MODE_INPUT_OUTPUT;
  io.pull_up_en = GPIO_PULLUP_DISABLE;
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&io) != ESP_OK) {
    r.error = "gpio_config_failed";
    return r;
  }

  const bool do_rx = r.mode != "tx";
  const bool do_l0st = (r.mode == "txrx_l0st" || r.mode == "l0st");
  const uint32_t period_us =
      r.rate_hz > 0 ? (1000000UL / r.rate_hz) : 10;
  const uint32_t n = kPathBGpioSynthCount;

  // Small RX buffer on stack/heap — 177 bytes fine.
  uint8_t rx[kPathBGpioSynthCount];
  memset(rx, 0, sizeof(rx));

  const int64_t t0 = esp_timer_get_time();
  for (uint32_t i = 0; i < n; i++) {
    applyState(dp, dm, kPathBGpioSynthStates[i]);
    if (do_rx) {
      // Sample after drive settles a few cycles.
      rx[i] = readState(dp, dm);
    }
    busyWaitUs(period_us > 0 ? period_us : 1);
  }
  // Idle J
  applyState(dp, dm, 1);
  const int64_t t1 = esp_timer_get_time();

  r.states_tx = n;
  r.duration_us = static_cast<uint32_t>(t1 - t0);
  if (r.duration_us > 0) {
    r.achieved_rate_hz =
        static_cast<uint32_t>((static_cast<uint64_t>(n) * 1000000ULL) /
                              r.duration_us);
  }

  if (do_rx) {
    r.states_rx = n;
    for (uint32_t i = 0; i < n; i++) {
      if (rx[i] != kPathBGpioSynthStates[i]) {
        r.mismatches++;
      }
    }
    r.loopback_ok = (r.mismatches == 0);
  } else {
    r.loopback_ok = true; // tx-only: no compare
  }

  if (do_l0st) {
    if (l0st_host.length() == 0) {
      r.error = "l0st_host_required";
      r.ok = r.loopback_ok;
      return r;
    }
    const uint8_t *payload = do_rx ? rx : kPathBGpioSynthStates;
    String err;
    r.l0st_ok = pushL0st(l0st_host, r.l0st_port, r.achieved_rate_hz
                                                     ? r.achieved_rate_hz
                                                     : r.rate_hz,
                         payload, n, &r.l0st_bytes, &err);
    if (!r.l0st_ok) {
      r.error = err;
    }
  }

  r.ok = r.loopback_ok && (!do_l0st || r.l0st_ok);
  return r;
}

} // namespace usb_analyzer
