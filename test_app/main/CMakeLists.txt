idf_component_register(
    SRCS
        "test_main.cpp"
        "test_bme280_sensor.cpp"
        "../../main/bme280_sensor.cpp"

    INCLUDE_DIRS
        "."
        "../../main"

    REQUIRES
        unity
        bme280_driver
        esp_driver_i2c
        freertos
        esp_timer

    WHOLE_ARCHIVE
)
