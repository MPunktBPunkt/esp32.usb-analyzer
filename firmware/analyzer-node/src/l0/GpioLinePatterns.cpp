#include "l0/GpioLinePatterns.h"
#include "l0/PathBGpioSynthStates.h"
#include "l0/WireStimDataBadCrc.h"
#include "l0/WireStimSetupBadCrc.h"
#include "l0/WireStimSetupOk.h"
#include "l0/WireStimSync.h"

#include <cstring>

namespace usb_analyzer {
namespace {

void appendHold(std::vector<uint8_t> &out, uint8_t st, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    out.push_back(st);
  }
}

uint32_t parseSuffix(const String &p, const char *prefix) {
  if (!p.startsWith(prefix)) {
    return 0;
  }
  return static_cast<uint32_t>(p.substring(strlen(prefix)).toInt());
}

} // namespace

bool isParametricPattern(const String &pattern) {
  return pattern.startsWith("gap_") || pattern.startsWith("glitch_se0_") ||
         pattern.startsWith("glitch_k_") || pattern.startsWith("hold_j_") ||
         pattern.startsWith("hold_se0_");
}

std::vector<uint8_t> buildGpioLinePattern(const String &pattern, String *err) {
  std::vector<uint8_t> out;
  const String p = pattern.length() ? pattern : "setup_ok";

  if (p == "bus_states") {
    appendHold(out, 0, 64);
    appendHold(out, 1, 64);
    appendHold(out, 2, 64);
    appendHold(out, 3, 32);
    appendHold(out, 1, 64);
  } else if (p == "sync") {
    for (int rep = 0; rep < 16; rep++) {
      out.insert(out.end(), kWireStim_Sync, kWireStim_Sync + kWireStim_Sync_Count);
      appendHold(out, 1, 8);
    }
  } else if (p == "eop") {
    appendHold(out, 1, 16);
    appendHold(out, 0, 2);
    appendHold(out, 1, 16);
  } else if (p == "se0_reset") {
    appendHold(out, 0, 200);
    appendHold(out, 1, 64);
  } else if (p == "setup_ok" || p == "synth") {
    out.assign(kPathBGpioSynthStates,
               kPathBGpioSynthStates + kPathBGpioSynthCount);
  } else if (p == "setup_ok_wire") {
    out.assign(kWireStim_SetupOk, kWireStim_SetupOk + kWireStim_SetupOk_Count);
  } else if (p == "setup_bad_crc") {
    out.assign(kWireStim_SetupBadCrc,
               kWireStim_SetupBadCrc + kWireStim_SetupBadCrc_Count);
  } else if (p == "data_bad_crc") {
    out.assign(kWireStim_DataBadCrc,
               kWireStim_DataBadCrc + kWireStim_DataBadCrc_Count);
  } else if (p == "glitch") {
    appendHold(out, 1, 32);
    out.push_back(2);
    out.push_back(1);
    appendHold(out, 1, 32);
    out.push_back(0);
    out.push_back(1);
    appendHold(out, 1, 32);
  } else if (p == "jk_burst") {
    for (int i = 0; i < 256; i++) {
      out.push_back((i & 1) ? 1 : 2);
    }
    appendHold(out, 1, 16);
  } else if (p.startsWith("gap_")) {
    const uint32_t gap = parseSuffix(p, "gap_");
    out.insert(out.end(), kWireStim_Sync, kWireStim_Sync + kWireStim_Sync_Count);
    appendHold(out, 1, gap ? gap : 8);
    out.insert(out.end(), kWireStim_Sync, kWireStim_Sync + kWireStim_Sync_Count);
    appendHold(out, 1, 16);
  } else if (p.startsWith("glitch_se0_")) {
    const uint32_t n = parseSuffix(p, "glitch_se0_");
    appendHold(out, 1, 32);
    appendHold(out, 0, n ? n : 1);
    appendHold(out, 1, 32);
  } else if (p.startsWith("glitch_k_")) {
    const uint32_t n = parseSuffix(p, "glitch_k_");
    appendHold(out, 1, 32);
    appendHold(out, 2, n ? n : 1);
    appendHold(out, 1, 32);
  } else if (p.startsWith("hold_j_")) {
    appendHold(out, 1, parseSuffix(p, "hold_j_") ? parseSuffix(p, "hold_j_") : 64);
  } else if (p.startsWith("hold_se0_")) {
    appendHold(out, 0,
               parseSuffix(p, "hold_se0_") ? parseSuffix(p, "hold_se0_") : 64);
    appendHold(out, 1, 16);
  } else if (p == "pid_bad") {
    // SYNC + illegal SE1 burst as stand-in for bad framing
    out.insert(out.end(), kWireStim_Sync, kWireStim_Sync + kWireStim_Sync_Count);
    appendHold(out, 3, 16);
    appendHold(out, 1, 16);
  } else {
    if (err) {
      *err = "unknown_pattern";
    }
  }
  return out;
}

} // namespace usb_analyzer
