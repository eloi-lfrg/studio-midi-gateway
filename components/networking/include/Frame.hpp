#pragma once

#include <array>
#include <cstdint>

namespace networking {

static constexpr size_t FRAME_PAYLOAD_MAX_SIZE = 250;

struct Frame {
  std::array<uint8_t, 6> srcMac;
  uint8_t                payload[FRAME_PAYLOAD_MAX_SIZE];
  size_t                 payloadLen;
};

} // namespace networking
