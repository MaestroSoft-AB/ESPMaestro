idf_component_register(
    SRCS "test_bme280_sensor.cpp"
    INCLUDE_DIRS "." ".."
    PRIV_REQUIRES unity esp_driver_i2c esp_timer freertos
)
