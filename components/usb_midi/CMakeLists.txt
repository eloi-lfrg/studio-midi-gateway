idf_component_register(
    SRCS
        "UsbMidi.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_REQUIRES
        espressif__esp_tinyusb
)
