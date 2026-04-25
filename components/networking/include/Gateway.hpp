#pragma once

#include <functional>

#include "Frame.hpp"
#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"

namespace networking {

class Gateway {
public:
  using OnFrameCb = std::function<void(const Frame &)>;

  /** @brief Construct the gateway.
   *  @param queueDepth Number of frames the receive queue can hold. */
  explicit Gateway(size_t queueDepth = 16);
  ~Gateway();

  Gateway(const Gateway &) = delete;
  Gateway &operator=(const Gateway &) = delete;

  /** @brief Initialise ESP-NOW and start the receive task.
   *  @return ESP_OK on success. */
  esp_err_t begin();

  /** @brief Register a callback invoked for every received frame.
   *  @param cb Callable — called from the receive task context. */
  void setOnFrameCb(OnFrameCb on_frame_cb);

private:
  static constexpr const char *TAG = "Gateway";

  // ── ESP-NOW callbacks ──
  static void IRAM_ATTR s_recvCb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len);

  // ── Task ──
  static void s_processEntry(void *arg);
  void processTask();

  // ── Members ──
  QueueHandle_t m_queue;
  TaskHandle_t m_task;
  OnFrameCb m_onFrame;
  const size_t m_queueDepth;

  static Gateway *s_instance;
};

} // namespace networking
