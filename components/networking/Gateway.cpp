#include "Gateway.hpp"

#include <algorithm>
#include <cstring>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/projdefs.h"
#include "portmacro.h"

namespace networking {

Gateway *Gateway::s_instance = nullptr;

// ─── Lifecycle ───────────────────────────────────────────────────────────────

Gateway::Gateway(size_t queueDepth)
    : m_queue(nullptr), m_onFrame(nullptr), m_onNodeCountChange(nullptr), m_queueDepth(queueDepth) {
  s_instance = this;
}

Gateway::~Gateway() {
  esp_now_unregister_recv_cb();
  esp_now_deinit();

  if (m_processTask) {
    vTaskDelete(m_processTask);
    m_processTask = nullptr;
  }

  if (m_pollTask) {
    vTaskDelete(m_pollTask);
    m_pollTask = nullptr;
  }

  if (m_queue) {
    vQueueDelete(m_queue);
    m_queue = nullptr;
  }

  s_instance = nullptr;
}

// ─── begin ───────────────────────────────────────────────────────────────────

esp_err_t Gateway::begin() {
  m_queue = xQueueCreate(m_queueDepth, sizeof(Frame));
  if (!m_queue) {
    ESP_LOGE(TAG, "Queue creation failed");
    return ESP_ERR_NO_MEM;
  }

  esp_err_t ret = esp_now_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  esp_now_register_recv_cb(s_recvCb);

  if (xTaskCreate(s_processEntry, "gw_process", 4096, this, 5, &m_processTask) != pdPASS) {
    ESP_LOGE(TAG, "Process task creation failed");
    return ESP_ERR_NO_MEM;
  }

  if (xTaskCreate(s_pollEntry, "gw_poll", 2048, this, 3, &m_pollTask) != pdPASS) {
    ESP_LOGE(TAG, "Poll task creation failed");
    return ESP_ERR_NO_MEM;
  }

  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return ESP_OK;
}

// ─── setConnectKey ───────────────────────────────────────────────────────────

void Gateway::setConnectKey(const std::array<uint8_t, CONNECT_KEY_LEN> &key) {
  m_connectKey = key;
  m_hasKey     = true;
}

// ─── setOnFrameCb ────────────────────────────────────────────────────────────

void Gateway::setOnFrameCb(OnFrameCb cb) { m_onFrame = std::move(cb); }

// ─── setOnNodeCountChangeCb ──────────────────────────────────────────────────

void Gateway::setOnNodeCountChangeCb(OnNodeCountChangeCb cb) { m_onNodeCountChange = std::move(cb); }

// ─── s_recvCb ────────────────────────────────────────────────────────────────

void IRAM_ATTR Gateway::s_recvCb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (!s_instance)
    return;

  Frame frame;
  std::memcpy(frame.srcMac.data(), info->src_addr, 6);
  frame.payloadLen = std::min(static_cast<size_t>(len), FRAME_PAYLOAD_MAX_SIZE);
  std::memcpy(frame.payload, data, frame.payloadLen);

  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(s_instance->m_queue, &frame, &woken);
  if (woken)
    portYIELD_FROM_ISR();
}

// ─── s_processEntry / processTask ────────────────────────────────────────────

void Gateway::s_processEntry(void *arg) { static_cast<Gateway *>(arg)->processTask(); }

void Gateway::processTask() {
  Frame frame;
  while (true) {
    if (xQueueReceive(m_queue, &frame, portMAX_DELAY) != pdTRUE)
      continue;

    ESP_LOGD(TAG, "rx %02X:%02X:%02X:%02X:%02X:%02X len=%zu", frame.srcMac[0], frame.srcMac[1],
             frame.srcMac[2], frame.srcMac[3], frame.srcMac[4], frame.srcMac[5], frame.payloadLen);

    if (frame.payloadLen > 0 && frame.payload[0] == FRAME_TYPE_CONNECT) {
      handleConnect(frame);
    } else if (m_onFrame) {
      m_onFrame(frame);
    }
  }
}

// ─── s_pollEntry / pollTask ──────────────────────────────────────────────────

void Gateway::s_pollEntry(void *arg) { static_cast<Gateway *>(arg)->pollTask(); 
}
void Gateway::pollTask() {
  uint32_t lastCount = 0;

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));

    uint32_t count = m_nodeCount;
    if (count == lastCount)
      continue;

    lastCount = count;
    ESP_LOGI(TAG, "node count changed: %lu", count);

    if (m_onNodeCountChange) {
      m_onNodeCountChange(count);
    }
  }
}

// ─── handleConnect ───────────────────────────────────────────────────────────

void Gateway::handleConnect(const Frame &frame) {
  uint8_t status = CONNACK_REFUSED;

  if (!m_hasKey) {
    ESP_LOGW(TAG, "connect received but no key configured — refusing");
  } else if (frame.payloadLen != 1 + CONNECT_KEY_LEN) {
    ESP_LOGW(TAG, "connect frame has wrong length (%zu) — refusing", frame.payloadLen);
  } else if (std::memcmp(frame.payload + 1, m_connectKey.data(), CONNECT_KEY_LEN) != 0) {
    ESP_LOGW(TAG, "connect from %02X:%02X:%02X:%02X:%02X:%02X — wrong key",
             frame.srcMac[0], frame.srcMac[1], frame.srcMac[2],
             frame.srcMac[3], frame.srcMac[4], frame.srcMac[5]);
  } else {
    status = CONNACK_ACCEPTED;
    ESP_LOGI(TAG, "connect accepted from %02X:%02X:%02X:%02X:%02X:%02X",
             frame.srcMac[0], frame.srcMac[1], frame.srcMac[2],
             frame.srcMac[3], frame.srcMac[4], frame.srcMac[5]);

    bool alreadyKnown = false;
    for (const auto &mac : m_connectedNodes) {
      if (mac == frame.srcMac) {
        alreadyKnown = true;
        break;
      }
    }
    if (!alreadyKnown) {
      m_connectedNodes.push_back(frame.srcMac);
      m_nodeCount = static_cast<uint32_t>(m_connectedNodes.size());
    }
  }

  if (!esp_now_is_peer_exist(frame.srcMac.data())) {
    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, frame.srcMac.data(), 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }

  uint8_t response[2] = {FRAME_TYPE_CONNACK, status};
  sendTo(frame.srcMac, response, sizeof(response));
}

// ─── sendTo ──────────────────────────────────────────────────────────────────

esp_err_t Gateway::sendTo(const std::array<uint8_t, 6> &mac, const uint8_t *data, size_t len) {
  esp_err_t ret = esp_now_send(mac.data(), data, len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(ret));
  }
  return ret;
}

} // namespace networking
