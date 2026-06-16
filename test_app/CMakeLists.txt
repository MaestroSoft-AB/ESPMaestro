cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components/bme280_driver"
    "${CMAKE_CURRENT_LIST_DIR}/../components/gpio"
    "${CMAKE_CURRENT_LIST_DIR}/../components/i2c"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESPMaestro_tests)
