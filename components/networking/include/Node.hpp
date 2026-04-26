#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "Frame.hpp"
#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/semphr.h"

namespace networking {

using OnConnectCb  = std::function<void(bool accepted)>;
using OnSendFailCb = std::function<void()>;

class Node {
public:
  explicit Node(const std::array<uint8_t, 6> &gatewayMac);

  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;

  /** @brief Initialise ESP-NOW and register the gateway peer. */
  esp_err_t begin();

  /** @brief Send a CONNECT frame and register a callback for the CONNACK.
   *  @param key Shared key (CONNECT_KEY_LEN bytes).
   *  @param cb  Called with true if the gateway accepted the connection. */
  esp_err_t connect(const std::array<uint8_t, CONNECT_KEY_LEN> &key, OnConnectCb cb);

  /** @brief Register a callback invoked when the RF-level send acknowledgement fails.
   *  Called from the WiFi task context — keep it short. */
  void setOnSendFailCb(OnSendFailCb cb);

  esp_err_t send(const uint8_t *data, size_t len);

private:
  static constexpr const char *TAG = "Node";

  static void       s_sendCb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);
  static void IRAM_ATTR s_recvCb(const esp_now_recv_info_t *info, const uint8_t *data, int len);

  static SemaphoreHandle_t s_txSem;
  static Node             *s_instance;

  std::array<uint8_t, 6> m_gatewayMac;
  OnConnectCb            m_connectCb{nullptr};
  OnSendFailCb           m_sendFailCb{nullptr};
};

} // namespace networking
