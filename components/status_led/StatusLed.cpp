#include "StatusLed.hpp"

#include "esp_log.h"
#include "freertos/projdefs.h"
#include "led_strip.h"

// ─── Lifecycle ───────────────────────────────────────────────────────────────

StatusLed::StatusLed(gpio_num_t gpio) : m_gpio(gpio) {}

// ─── setNodeCount ────────────────────────────────────────────────────────────

void StatusLed::setNodeCount(uint32_t count) {
  if (m_task) {
    xTaskNotify(m_task, count, eSetValueWithOverwrite);
  }
}

// ─── begin ───────────────────────────────────────────────────────────────────

esp_err_t StatusLed::begin() {
  led_strip_config_t stripCfg{};
  stripCfg.strip_gpio_num         = static_cast<int>(m_gpio);
  stripCfg.max_leds               = 1;
  stripCfg.led_model              = LED_MODEL_WS2812;
  stripCfg.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;

  led_strip_rmt_config_t rmtCfg{};
  rmtCfg.resolution_hz = 10 * 1000 * 1000;

  esp_err_t ret = led_strip_new_rmt_device(&stripCfg, &rmtCfg, &m_strip);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(ret));
    return ret;
  }

  led_strip_clear(m_strip);

  BaseType_t created = xTaskCreate(s_ledEntry, "status_led", 2048, this, 3, &m_task);
  if (created != pdPASS) {
    ESP_LOGE(TAG, "Task creation failed");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

// ─── s_ledEntry ──────────────────────────────────────────────────────────────

void StatusLed::s_ledEntry(void *arg) { static_cast<StatusLed *>(arg)->ledTask(); }

// ─── ledTask ─────────────────────────────────────────────────────────────────
//
// States:
//   count == 0 : continuous red blink (interruptible by notification)
//   count  > 0 : solid green, wait for next notification
//
// On any count change: blink 3x with the target colour as transition animation.

void StatusLed::ledTask() {
  uint32_t count = 0;

  while (true) {
    if (count == 0) {
      uint32_t notified = 0;

      setColor(32, 0, 0);
      if (xTaskNotifyWait(0, UINT32_MAX, &notified, pdMS_TO_TICKS(BLINK_ON_MS)) == pdTRUE) {
        count = notified;
        blink(0, 32, 0, TRANS_BLINKS);
        setColor(0, 32, 0);
        continue;
      }

      setColor(0, 0, 0);
      if (xTaskNotifyWait(0, UINT32_MAX, &notified, pdMS_TO_TICKS(BLINK_OFF_MS)) == pdTRUE) {
        count = notified;
        blink(0, 32, 0, TRANS_BLINKS);
        setColor(0, 32, 0);
        continue;
      }
    } else {
      uint32_t notified = 0;
      setColor(0, 32, 0);
      if (xTaskNotifyWait(0, UINT32_MAX, &notified, portMAX_DELAY) == pdTRUE) {
        count = notified;
        if (count == 0) {
          blink(32, 0, 0, TRANS_BLINKS);
          // next iteration starts continuous red blink
        } else {
          blink(0, 32, 0, TRANS_BLINKS);
          setColor(0, 32, 0);
        }
      }
    }
  }
}

// ─── setColor ────────────────────────────────────────────────────────────────

void StatusLed::setColor(uint8_t r, uint8_t g, uint8_t b) {
  led_strip_set_pixel(m_strip, 0, r, g, b);
  led_strip_refresh(m_strip);
}

// ─── blink ───────────────────────────────────────────────────────────────────

void StatusLed::blink(uint8_t r, uint8_t g, uint8_t b, size_t times) {
  for (size_t i = 0; i < times; ++i) {
    setColor(r, g, b);
    vTaskDelay(pdMS_TO_TICKS(BLINK_ON_MS));
    setColor(0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_OFF_MS));
  }
}
