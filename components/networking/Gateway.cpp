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
    : m_queue(nullptr), m_onFrame(nullptr), m_queueDepth(queueDepth) {
  s_instance = this;
}

Gateway::~Gateway() {
  esp_now_unregister_recv_cb();
  esp_now_deinit();

  if (m_processTask) {
    vTaskDelete(m_processTask);
    m_processTask = nullptr;
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

  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  ESP_LOGI(TAG, "ready — MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return ESP_OK;
}

// ─── setOnFrameCb ────────────────────────────────────────────────────────────

void Gateway::setOnFrameCb(OnFrameCb cb) { m_onFrame = std::move(cb); }

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

    if (m_onFrame)
      m_onFrame(frame);
  }
}

} // namespace networking
