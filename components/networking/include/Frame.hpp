#pragma once

#include <array>
#include <cstdint>

namespace networking {

static constexpr size_t FRAME_PAYLOAD_MAX_SIZE = 250;

// ── Frame types ──────────────────────────────────────────────────────────────
static constexpr uint8_t FRAME_TYPE_CONNECT = 0xC0;
static constexpr uint8_t FRAME_TYPE_CONNACK = 0xC1;

// CONNACK status codes (0x00 = accepted, matches MQTT/BLE conventions)
static constexpr uint8_t CONNACK_ACCEPTED = 0x00;
static constexpr uint8_t CONNACK_REFUSED  = 0x01;

static constexpr size_t CONNECT_KEY_LEN = 4;

struct Frame {
  std::array<uint8_t, 6> srcMac;
  uint8_t                payload[FRAME_PAYLOAD_MAX_SIZE];
  size_t                 payloadLen;
};

} // namespace networking
