#pragma once

#include <functional>
#include <vector> // IWYU pragma: keep

#include "Frame.hpp"
#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"

namespace networking {

class Gateway {
public:
  using OnFrameCb           = std::function<void(const Frame &)>;
  using OnNodeCountChangeCb = std::function<void(uint32_t count)>;

  /** @brief Construct the gateway.
   *  @param queueDepth Number of frames the receive queue can hold. */
  explicit Gateway(size_t queueDepth = 16);
  ~Gateway();

  Gateway(const Gateway &) = delete;
  Gateway &operator=(const Gateway &) = delete;

  /** @brief Initialise ESP-NOW and start the receive and poll tasks.
   *  @return ESP_OK on success. */
  esp_err_t begin();

  /** @brief Set the shared key used to authenticate CONNECT requests.
   *  Must be called before begin(). */
  void setConnectKey(const std::array<uint8_t, CONNECT_KEY_LEN> &key);

  /** @brief Register a callback invoked for every received data frame.
   *  @param cb Called from the receive task context. */
  void setOnFrameCb(OnFrameCb cb);

  /** @brief Register a callback invoked when the connected node count changes.
   *  Fired by the poll task every 3 s on change (connect or disconnect).
   *  @param cb Called with the new node count. */
  void setOnNodeCountChangeCb(OnNodeCountChangeCb cb);

private:
  static constexpr const char *TAG              = "Gateway";
  static constexpr uint32_t    POLL_INTERVAL_MS = 3000;

  // ── ESP-NOW callback ──
  static void IRAM_ATTR s_recvCb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len);

  // ── Tasks ──
  static void s_processEntry(void *arg);
  static void s_pollEntry(void *arg);
  void processTask();
  void pollTask();

  esp_err_t sendTo(const std::array<uint8_t, 6> &mac, const uint8_t *data, size_t len);
  void      handleConnect(const Frame &frame);

  // ── Members ──
  QueueHandle_t               m_queue;
  TaskHandle_t                m_processTask{nullptr};
  TaskHandle_t                m_pollTask{nullptr};
  OnFrameCb                   m_onFrame;
  OnNodeCountChangeCb         m_onNodeCountChange;
  const size_t                m_queueDepth;

  std::array<uint8_t, CONNECT_KEY_LEN>  m_connectKey{};
  bool                                  m_hasKey{false};
  std::vector<std::array<uint8_t, 6>>   m_connectedNodes;
  volatile uint32_t                     m_nodeCount{0};

  static Gateway *s_instance;
};

} // namespace networking
