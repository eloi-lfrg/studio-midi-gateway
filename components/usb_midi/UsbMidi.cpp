#include "UsbMidi.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"

namespace midi {

// ── USB descriptors ───────────────────────────────────────────────────────────

enum ItfNum {
    ITF_NUM_MIDI = 0,
    ITF_NUM_MIDI_STREAMING,
    ITF_COUNT,
};

enum UsbEp {
    EP_EMPTY  = 0,
    EPNUM_MIDI = 1,
};

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

static const char *s_strDesc[] = {
    (char[]){0x09, 0x04},      // 0: language — English
    "Studio",                  // 1: manufacturer
    "MIDI Gateway",            // 2: product
    "000001",                  // 3: serial
    "MIDI Gateway Port",       // 4: MIDI interface
};

static const uint8_t s_cfgDesc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESC_TOTAL_LEN, 0, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64),
};

#if TUD_OPT_HIGH_SPEED
static const uint8_t s_cfgDescHs[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESC_TOTAL_LEN, 0, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 512),
};
#endif

// ─── begin ───────────────────────────────────────────────────────────────────

esp_err_t UsbMidi::begin() {
    tinyusb_config_t cfg = TINYUSB_DEFAULT_CONFIG();

    cfg.descriptor.string       = s_strDesc;
    cfg.descriptor.string_count = sizeof(s_strDesc) / sizeof(s_strDesc[0]);
    cfg.descriptor.full_speed_config = s_cfgDesc;
#if TUD_OPT_HIGH_SPEED
    cfg.descriptor.high_speed_config = s_cfgDescHs;
    cfg.descriptor.qualifier         = nullptr;
#endif

    esp_err_t ret = tinyusb_driver_install(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    xTaskCreate(rxTaskEntry, "usb_midi_rx", 4096, this, 5, nullptr);
    ESP_LOGI(TAG, "USB MIDI ready");
    return ESP_OK;
}

// ─── sendCc ──────────────────────────────────────────────────────────────────

void UsbMidi::sendCc(uint8_t channel, uint8_t cc, uint8_t value) {
    if (!tud_midi_mounted()) {
        return;
    }
    // channel is 1-based on the wire; USB MIDI uses 0-based
    const uint8_t status             = 0xB0 | ((channel - 1) & 0x0F);
    const uint8_t msg[3]             = {status, cc, value};
    tud_midi_stream_write(0, msg, sizeof(msg));
}

// ─── rxTask ──────────────────────────────────────────────────────────────────

void UsbMidi::rxTaskEntry(void *arg) {
    static_cast<UsbMidi *>(arg)->rxTask();
}

void UsbMidi::rxTask() {
    uint8_t packet[4];
    for (;;) {
        vTaskDelay(1);
        while (tud_midi_available()) {
            if (tud_midi_packet_read(packet)) {
                ESP_LOGD(TAG, "rx %02X %02X %02X %02X", packet[0], packet[1], packet[2], packet[3]);
            }
        }
    }
}

} // namespace midi
