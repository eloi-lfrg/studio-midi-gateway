#pragma once

#include <array>
#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep

namespace networking {

class Node {
public:
  explicit Node(const std::array<uint8_t, 6> &gatewayMac);

  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;

  /** @brief Initialise ESP-NOW and register the gateway peer. */
  esp_err_t begin(uint8_t channel);

  /** @brief Send raw data to the gateway (fire-and-forget). */
  esp_err_t send(const uint8_t *data, size_t len);

private:
  static constexpr const char *TAG = "Node";

  std::array<uint8_t, 6> m_gatewayMac;
};

} // namespace networking
