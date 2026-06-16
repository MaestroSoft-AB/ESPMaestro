idf_component_register(
    SRCS
        "test_main.cpp"
        "test_bme280_sensor.cpp"
        "test_scheduler.c"
        "test_ui_status.cpp"
        "../../main/bme280_sensor.cpp"
        "../../main/scheduler.c"
        "../../main/ui_status.cpp"

    INCLUDE_DIRS
        "."
        "../../main"

    REQUIRES
        unity
        bme280_driver
        esp_netif
        esp_wifi
        esp_driver_i2c
        freertos
        esp_timer
        gpio
        i2c
        lwip
        nvs_flash

    WHOLE_ARCHIVE
)
