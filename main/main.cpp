#include "Gateway.hpp"
#include "StatusLed.hpp"
#include "UsbMidi.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static constexpr const char *TAG = "main";

static constexpr uint8_t NETWORKING_WIFI_CHANNEL = 1;

static networking::Gateway gateway;
static midi::UsbMidi usbMidi;
static StatusLed statusLed;

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

  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_AP, mac);
  ESP_LOGI(TAG, "WiFi started (AP) — AP MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);

  uint8_t primary;
  wifi_second_chan_t secundary;
  ESP_ERROR_CHECK(esp_wifi_get_channel(&primary, &secundary));
  ESP_LOGI(TAG, "WiFi channel: primary=%u secundary=%d", primary, secundary);
}

static void onFrameReceived(const networking::Frame &frame) {
  ESP_LOGI(TAG, "received frame from %02X:%02X:%02X:%02X:%02X:%02X (len=%zu)", frame.srcMac[0], frame.srcMac[1],
           frame.srcMac[2], frame.srcMac[3], frame.srcMac[4], frame.srcMac[5], frame.payloadLen);

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

  gateway.setOnFrameCb(onFrameReceived);
  ESP_ERROR_CHECK(gateway.begin());
}
