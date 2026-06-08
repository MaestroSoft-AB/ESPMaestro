idf_component_register(SRCS "esp_maestro.cpp" "fonts/notosans_14p.c" "display_handler.c" "wifi_handler.c" "ui.c" "time_utils.c" "scheduler.c" "ui_status.cpp" "facility_config.c" "dashboard_data.cpp" "bme280_sensor.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_client json esp_netif esp_wifi nvs_flash lvgl__lvgl driver bme280_driver i2c
                    PRIV_REQUIRES touch lvgl_port rgb_lcd_port esp_lcd)
