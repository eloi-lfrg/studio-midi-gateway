#include "Node.hpp"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"

namespace networking {

SemaphoreHandle_t Node::s_txSem    = nullptr;
Node             *Node::s_instance = nullptr;

// ─── Lifecycle ───────────────────────────────────────────────────────────────

Node::Node(const std::array<uint8_t, 6> &gatewayMac) : m_gatewayMac(gatewayMac) {
  s_instance = this;
}

// ─── begin ───────────────────────────────────────────────────────────────────

esp_err_t Node::begin() {
  s_txSem = xSemaphoreCreateBinary();
  xSemaphoreGive(s_txSem);

  esp_err_t ret = esp_now_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  esp_now_register_send_cb(s_sendCb);

  esp_now_peer_info_t peer{};
  std::memcpy(peer.peer_addr, m_gatewayMac.data(), 6);
  peer.channel = 0;
  peer.encrypt = false;

  ret = esp_now_add_peer(&peer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_add_peer failed: %s", esp_err_to_name(ret));
  }
  return ret;
}

// ─── setOnSendFailCb ─────────────────────────────────────────────────────────

void Node::setOnSendFailCb(OnSendFailCb cb) { m_sendFailCb = std::move(cb); }

// ─── connect ─────────────────────────────────────────────────────────────────

esp_err_t Node::connect(const std::array<uint8_t, CONNECT_KEY_LEN> &key, OnConnectCb cb) {
  m_connectCb = std::move(cb);
  esp_now_register_recv_cb(s_recvCb);

  uint8_t payload[1 + CONNECT_KEY_LEN];
  payload[0] = FRAME_TYPE_CONNECT;
  std::memcpy(payload + 1, key.data(), CONNECT_KEY_LEN);

  return send(payload, sizeof(payload));
}

// ─── send ────────────────────────────────────────────────────────────────────

esp_err_t Node::send(const uint8_t *data, size_t len) {
  if (xSemaphoreTake(s_txSem, pdMS_TO_TICKS(50)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  esp_err_t ret = esp_now_send(m_gatewayMac.data(), data, len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(ret));
    xSemaphoreGive(s_txSem);
  }
  return ret;
}

// ─── s_sendCb ────────────────────────────────────────────────────────────────

void Node::s_sendCb(const wifi_tx_info_t * /*tx_info*/, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    ESP_LOGW(TAG, "send failed");
    if (s_instance && s_instance->m_sendFailCb) {
      s_instance->m_sendFailCb();
    }
  }
  xSemaphoreGive(s_txSem);
}

// ─── s_recvCb ────────────────────────────────────────────────────────────────

void IRAM_ATTR Node::s_recvCb(const esp_now_recv_info_t * /*info*/, const uint8_t *data, int len) {
  if (!s_instance || len < 2) return;
  if (data[0] != FRAME_TYPE_CONNACK) return;
  if (s_instance->m_connectCb) {
    s_instance->m_connectCb(data[1] == CONNACK_ACCEPTED);
  }
}

} // namespace networking
