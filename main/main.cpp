#include "Gateway.hpp"

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

static networking::Gateway gateway;

extern "C" void app_main() {
  wifiInit();

  gateway.setOnFrameCb(
      [](const networking::Frame &frame) { ESP_LOGI(TAG, "frame received len=%zu", frame.payloadLen); });

  ESP_ERROR_CHECK(gateway.begin());
}
