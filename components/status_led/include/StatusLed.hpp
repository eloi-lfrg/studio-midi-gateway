#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "led_strip.h"        // IWYU pragma: keep

class StatusLed {
public:
  /** @brief Construct the LED driver.
   *  @param gpio GPIO number of the WS2812 data line (GPIO 48 on ESP32-S3-DevKitC-1). */
  explicit StatusLed(gpio_num_t gpio = GPIO_NUM_48);

  /** @brief Initialise the LED strip and start the LED task.
   *  @return ESP_OK on success. */
  esp_err_t begin();

  /** @brief Update the connected node count and wake the LED task.
   *  Safe to call from any task context.
   *  @param count New number of connected nodes. */
  void setNodeCount(uint32_t count);

private:
  static constexpr const char *TAG         = "StatusLed";
  static constexpr uint32_t BLINK_ON_MS   = 300;
  static constexpr uint32_t BLINK_OFF_MS  = 300;
  static constexpr uint32_t TRANS_BLINKS  = 3;

  static void s_ledEntry(void *arg);
  void ledTask();

  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void blink(uint8_t r, uint8_t g, uint8_t b, size_t times);

  gpio_num_t         m_gpio;
  TaskHandle_t       m_task{nullptr};
  led_strip_handle_t m_strip{nullptr};
};
