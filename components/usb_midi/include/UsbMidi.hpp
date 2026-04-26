#pragma once

#include "esp_err.h"
#include <cstdint>

namespace midi {

class UsbMidi {
public:
    esp_err_t begin();
    void      sendCc(uint8_t channel, uint8_t cc, uint8_t value);

private:
    static constexpr const char *TAG = "UsbMidi";

    static void rxTaskEntry(void *arg);
    void        rxTask();
};

} // namespace midi
