#pragma once

#include <Arduino.h>
#include <cstring>

namespace usb_analyzer {

enum class Role : uint8_t {
  Unassigned = 0,
  Host = 1,
  Device = 2,
};

inline const char *roleName(Role r) {
  switch (r) {
  case Role::Host:
    return "host";
  case Role::Device:
    return "device";
  default:
    return "unassigned";
  }
}

inline Role roleFromName(const char *name) {
  if (!name) {
    return Role::Unassigned;
  }
  if (strcmp(name, "host") == 0) {
    return Role::Host;
  }
  if (strcmp(name, "device") == 0) {
    return Role::Device;
  }
  return Role::Unassigned;
}

} // namespace usb_analyzer
