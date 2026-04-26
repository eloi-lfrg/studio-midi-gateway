idf_component_register(SRCS "main.cpp"
                       PRIV_REQUIRES
                          nvs_flash
                          esp_wifi
                          networking
                          usb_midi
                          led_strip
                          status_led
                       INCLUDE_DIRS ".")
