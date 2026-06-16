# ESPMaestro

A smart home dashboard for the **Waveshare ESP32-S3-Touch-LCD-7B** (1024×600, 7-inch capacitive touch display). It shows real-time electricity prices and consumption, indoor climate data, weather forecasts, and current time — all fetched from an OptiMaestro backend service.

## Features

- Live electricity spot prices (SE1–SE4 price zones) with configurable time ranges
- Real-time power consumption graph
- Indoor climate monitoring via BME280 sensor (temperature, humidity, pressure)
- Outdoor weather and 24-hour forecast
- NTP-synchronized clock
- Touch-based setup wizard for Wi-Fi and facility configuration
- Serial CLI for configuration and debugging
- NVS-backed cache so the display shows useful data immediately on boot

## Hardware

| Component | Description |
|---|---|
| [Waveshare ESP32-S3-Touch-LCD-7B](https://www.waveshare.com/esp32-s3-touch-lcd-7b.htm) | Main board with 7" 1024×600 touch display |
| BME280 | Temperature, humidity and pressure sensor (connected via I2C) |

## Prerequisites

### ESP-IDF

Install **ESP-IDF v5.1.0 or later** (tested with v5.5.4).

Follow the [official installation guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) for your platform, then source the environment before building:

```bash
. $HOME/esp/esp-idf/export.sh
```

### Python 3

Required by the ESP-IDF build system. No additional Python packages are needed for the main firmware.

### LVGL v8

LVGL 8.4.0 is bundled as a local component under `components/lvgl_port/lvgl__lvgl/` and is picked up automatically by the build system.

## Building

Clone the repository and enter the project directory:

```bash
git clone <repo-url>
cd ESPMaestro
```

Configure the target (only needed once):

```bash
idf.py set-target esp32s3
```

Build the firmware:

```bash
idf.py build
```

## Flashing

Connect the board via USB, then flash and open the serial monitor in one step:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with the correct port for your system (e.g. `/dev/ttyACM0` on Linux or `COM3` on Windows).

## First-time Setup

On first boot — or whenever Wi-Fi or facility configuration is missing — the device shows an on-screen **setup wizard**. Use the touch display to:

1. Scan for and connect to a Wi-Fi network.
2. Enter your **facility name**, **GPS coordinates** (latitude/longitude), and **electricity price zone** (SE1–SE4).

Configuration is persisted in NVS flash and survives reboots.

## Serial CLI

The device exposes a command-line interface over the USB serial port (115 200 baud). Open the monitor with:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

or use any terminal emulator at 115200 8N1.

## Configuration

### sdkconfig

Project-specific defaults are stored in `sdkconfig.defaults` and are applied automatically on first configure. Notable settings:

| Key | Value | Notes |
|---|---|---|
| `CONFIG_IDF_TARGET` | `esp32s3` | Target chip |
| `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` | `y` | 16 MB flash |
| `CONFIG_SPIRAM` | `y` | PSRAM enabled (Octal, 80 MHz) |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240` | `y` | 240 MHz CPU |

Run `idf.py menuconfig` to change any setting interactively.

### Backend URL

The OptiMaestro backend URL is compiled in. To point at a different server, define the following preprocessor macros before building:

```
OPTIMAESTRO_DISPLAY_CURRENT_URL   – live power endpoint
OPTIMAESTRO_FACILITY_CONFIG_URL   – facility sync endpoint
```

These can be set via `idf.py menuconfig` → *Component config → ESPMaestro* or by adding `-D` flags in the project `CMakeLists.txt`.

## Running Tests

The unit tests live in `test_app/` and run as a separate ESP-IDF project on the same target:

```bash
cd test_app
idf.py set-target esp32s3
idf.py -p /dev/ttyUSB0 flash monitor
```

Test results are printed to the serial monitor. Currently the test suite covers the BME280 sensor driver.

## Project Structure

```
ESPMaestro/
├── main/                  # Application source (C/C++)
│   ├── esp_maestro.cpp    # Entry point (app_main)
│   ├── dashboard_data.*   # Data fetch state machine & NVS cache
│   ├── display_handler.*  # LVGL display driver & update pipeline
│   ├── bme280_sensor.*    # BME280 sensor wrapper
│   ├── wifi_handler.*     # Wi-Fi connection manager
│   ├── facility_config.*  # Facility identity & NVS persistence
│   ├── ntp_clock.hpp      # NTP time synchronization
│   ├── scheduler.*        # Lightweight cooperative task scheduler
│   ├── cli.*              # Serial command-line interface
│   └── ui.*               # LVGL UI screens
├── components/            # Local ESP-IDF components
│   ├── lvgl_port/         # LVGL port for the RGB LCD + GT911 touch
│   ├── bme280_driver/     # Bosch BME280 driver
│   └── …                  # LCD, I2C, fonts, GPIO helpers
├── test_app/              # Unit test project
├── SquareLine/            # SquareLine Studio UI project cache
├── sdkconfig.defaults     # Default build configuration
└── Doxyfile               # Doxygen documentation config
```

## Generating Documentation

API documentation is generated with Doxygen. Pre-built HTML is available in `html/`. To regenerate:

```bash
doxygen Doxyfile
```

Open `html/index.html` in a browser to browse the API docs.

## License

See [LICENSE](LICENSE) for details.
