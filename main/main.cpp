#include "Gateway.hpp"
#include "StatusLed.hpp"
#include "UsbMidi.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NETWORKING_WIFI_CHANNEL 1

static constexpr const char *TAG = "main";

static networking::Gateway gateway;
static midi::UsbMidi usbMidi;
static StatusLed statusLed;
static constexpr std::array<uint8_t, networking::CONNECT_KEY_LEN> CONNECT_KEY = {0x4B, 0x4E, 0x4F, 0x42};

static esp_err_t nvs_init() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  return ret;
}

static void wifiInit() {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(nvs_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_channel(NETWORKING_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

static void onFrameReceived(const networking::Frame &frame) {
  if (frame.payloadLen < 3) {
    ESP_LOGW(TAG, "frame too short: len=%zu", frame.payloadLen);
    return;
  }

  const uint8_t status = frame.payload[0];
  const uint8_t cc = frame.payload[1];
  const uint8_t value = frame.payload[2];

  if ((status & 0xF0) != 0xB0) {
    ESP_LOGW(TAG, "unexpected MIDI status: 0x%02X", status);
    return;
  }

  const uint8_t channel = (status & 0x0F) + 1;
  ESP_LOGI(TAG, "CC ch=%u cc=%u value=%u", channel, cc, value);
  usbMidi.sendCc(channel, cc, value);
}

extern "C" void app_main() {
  wifiInit();
  ESP_ERROR_CHECK(usbMidi.begin());
  ESP_ERROR_CHECK(statusLed.begin());

  gateway.setOnFrameCb(onFrameReceived);
  gateway.setOnNodeCountChangeCb([](uint32_t count) {
    ESP_LOGI(TAG, "Node count changed: %lu", count);
    statusLed.setNodeCount(count);
  });
  gateway.setConnectKey(CONNECT_KEY);
  ESP_ERROR_CHECK(gateway.begin());
}
