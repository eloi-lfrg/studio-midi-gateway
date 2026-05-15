#include "Node.hpp"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_now.h"

namespace networking {

// ─── Lifecycle ───────────────────────────────────────────────────────────────

Node::Node(const std::array<uint8_t, 6> &gatewayMac) : m_gatewayMac(gatewayMac) {}

// ─── begin ───────────────────────────────────────────────────────────────────

esp_err_t Node::begin(uint8_t channel) {
  esp_err_t ret = esp_now_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  esp_now_peer_info_t peer{};
  std::memcpy(peer.peer_addr, m_gatewayMac.data(), 6);
  peer.channel = channel;
  peer.encrypt = false;

  ret = esp_now_add_peer(&peer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_add_peer failed: %s", esp_err_to_name(ret));
  }
  return ret;
}

// ─── send ────────────────────────────────────────────────────────────────────

esp_err_t Node::send(const uint8_t *data, size_t len) {
  esp_err_t ret = esp_now_send(m_gatewayMac.data(), data, len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(ret));
  }
  return ret;
}

} // namespace networking
