#pragma once

#include <array>
#include <cstdint>

#include "esp_err.h"
#include "esp_now.h"

namespace networking {

class Node {
public:
  explicit Node(const std::array<uint8_t, 6> &gatewayMac);

  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;

  esp_err_t begin();
  esp_err_t send(const uint8_t *data, size_t len);

private:
  static constexpr const char *TAG = "Node";
  static void s_sendCb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);

  std::array<uint8_t, 6> m_gatewayMac;
};

} // namespace networking
