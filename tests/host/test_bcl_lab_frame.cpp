/**
 * Host unit tests for firmware/common/include/usb_analyzer/BclLabFrame.h
 * No ESP / USB hardware. Run: tests/host/run_bcl_lab_frame_tests.sh
 */
#include "usb_analyzer/BclLabFrame.h"

#include <cstdio>
#include <cstring>

using namespace usb_analyzer;

static int g_fail = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);       \
      g_fail++;                                                                \
    }                                                                          \
  } while (0)

static void test_roundtrip_handshake() {
  BclLabHeader h{};
  h.command = static_cast<uint16_t>(BclCmd::Handshake);
  h.src = 0x0001;
  h.dest = 0x0FA4;
  const uint8_t pl[6] = {0x00, 0x01, 0x1A, 0xB1, 0x10, 0x00};
  h.data_len = sizeof(pl);
  uint8_t frame[64];
  const size_t n = bclPack(frame, sizeof(frame), h, pl, sizeof(pl));
  CHECK(n == 14, "pack handshake len");
  BclLabHeader got{};
  CHECK(bclValidateFrame(frame, n, got), "validate handshake");
  CHECK(got.command == 4 && got.dest == 0x0FA4 && got.src == 1 &&
            got.data_len == 6,
        "handshake fields");
  CHECK(std::memcmp(frame + 8, pl, 6) == 0, "payload bytes");
}

static void test_empty_payload() {
  BclLabHeader h{};
  h.command = static_cast<uint16_t>(BclCmd::Open);
  h.src = 1;
  h.dest = 0x0FA4;
  h.data_len = 0;
  uint8_t frame[16];
  const size_t n = bclPack(frame, sizeof(frame), h, nullptr, 0);
  CHECK(n == 8, "header-only pack");
  BclLabHeader got{};
  CHECK(bclValidateFrame(frame, 8, got), "validate empty");
  CHECK(got.data_len == 0 && got.command == 1, "empty fields");
}

static void test_data_max_in_64() {
  BclLabHeader h{};
  h.command = static_cast<uint16_t>(BclCmd::Data);
  h.src = 2;
  h.dest = 3;
  uint8_t pl[56];
  for (size_t i = 0; i < sizeof(pl); i++)
    pl[i] = static_cast<uint8_t>(i);
  h.data_len = sizeof(pl);
  uint8_t frame[64];
  const size_t n = bclPack(frame, sizeof(frame), h, pl, sizeof(pl));
  CHECK(n == 64, "max frame");
  BclLabHeader got{};
  CHECK(bclValidateFrame(frame, n, got), "validate max");
  CHECK(got.data_len == 56, "max data_len");
}

static void test_negatives() {
  BclLabHeader h{};
  CHECK(!bclParseHeader(nullptr, 8, h), "null buf");
  uint8_t shortb[7] = {0};
  CHECK(!bclParseHeader(shortb, 7, h), "short <8");
  CHECK(!bclValidateFrame(shortb, 7, h), "validate short");

  uint8_t hdr[8] = {0x00, 0x02, 0x00, 0x01, 0x0F, 0xA4, 0x00, 0x04}; // DATA len=4
  CHECK(bclParseHeader(hdr, 8, h), "parse hdr alone");
  CHECK(!bclValidateFrame(hdr, 8, h), "validate truncated payload");
  CHECK(h.data_len == 4, "data_len still 4");

  uint8_t over[13];
  std::memcpy(over, hdr, 8);
  over[8] = over[9] = over[10] = over[11] = over[12] = 0xAA;
  CHECK(bclValidateFrame(over, 12, h), "exact 8+4");
  CHECK(!bclValidateFrame(over, 11, h), "one byte short");
  CHECK(!bclValidateFrame(over, 13, h), "one byte long");

  // pack mismatch payload_len vs data_len
  BclLabHeader bad{};
  bad.command = 2;
  bad.data_len = 3;
  uint8_t out[16];
  const uint8_t two[2] = {1, 2};
  CHECK(bclPack(out, sizeof(out), bad, two, 2) == 0, "pack len mismatch");
  CHECK(bclPack(out, 8, bad, nullptr, 0) == 0, "pack capacity when data_len!=0");
  bad.data_len = 0;
  CHECK(bclPack(out, 7, bad, nullptr, 0) == 0, "pack tiny capacity");

  // overflow claim: data_len huge vs buffer
  uint8_t huge[8] = {0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF};
  CHECK(bclParseHeader(huge, 8, h), "parse huge len");
  CHECK(!bclValidateFrame(huge, 8, h), "validate huge without body");
}

static void test_find_resync() {
  uint8_t buf[32];
  std::memset(buf, 0xEE, sizeof(buf));
  BclLabHeader h{};
  h.command = 4;
  h.src = 1;
  h.dest = 0x0FA4;
  h.data_len = 0;
  bclPack(buf + 5, sizeof(buf) - 5, h, nullptr, 0);
  size_t off = 99;
  BclLabHeader got{};
  CHECK(bclFindHeader(buf, sizeof(buf), off, got), "find after garbage");
  CHECK(off == 5, "offset 5");
  CHECK(got.command == 4 && got.dest == 0x0FA4, "found handshake");

  uint8_t junk[16];
  std::memset(junk, 0xFF, sizeof(junk));
  // all 0xFF → data_len=0xFFFF → need >64 → reject
  CHECK(!bclFindHeader(junk, sizeof(junk), off, got), "no false positive FFFF");
}

static void test_cmd_names() {
  CHECK(std::strcmp(bclCmdName(1), "OPEN") == 0, "OPEN");
  CHECK(std::strcmp(bclCmdName(2), "DATA") == 0, "DATA");
  CHECK(std::strcmp(bclCmdName(6), "DATAACK") == 0, "DATAACK");
  CHECK(std::strcmp(bclCmdName(99), "CMD") == 0, "unknown");
  CHECK(kBclLabInit[0] == 0x12 && kBclLabInit[3] == 0x78, "init magic");
}

int main() {
  test_roundtrip_handshake();
  test_empty_payload();
  test_data_max_in_64();
  test_negatives();
  test_find_resync();
  test_cmd_names();
  if (g_fail) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fail);
    return 1;
  }
  std::puts("OK BclLabFrame host unit tests");
  return 0;
}
