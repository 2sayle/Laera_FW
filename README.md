# Laera_FW

**Laera** is a connected device (IoT) monitoring the air quality using an ESP32 and Bosch's BME680 sensor.

## Overview

Laera_FW is an ESP-IDF based firmware project for environmental monitoring. It reads temperature, humidity, pressure, and air quality data from the BME680 sensor and transmits it over a network connection for remote monitoring.

## Key Features

- Real-time air quality monitoring
- Low-power IoT device
- Modular task-based architecture
- Sensor driver included


ESP-IDF firmware for the Laera project. The main code lives in `main/` and includes the BME680 (BME68x) driver.

## Prerequisites
- ESP-IDF v4.x or v5.x installed and configured (`IDF_PATH` variable)
- `idf.py` available in your PATH

## Structure
- `main/` : entry point (`app_main.c`) and application logic
- `drivers/bme680/` : BME680/BME68x sensor driver
- `tasks/` : application tasks
- `sdkconfig` : ESP-IDF configuration

## Build / Flash / Monitor
From the project root:

```sh
idf.py build
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
```

Adjust the serial port for your machine.

## Configuration
- The include paths for the `main` component are declared in `main/CMakeLists.txt` via `INCLUDE_DIRS`.
- The `BME68X_USE_INT` flag is defined globally in `CMakeLists.txt`.

## Notes
- If you add new headers, make sure their folder is listed in `INCLUDE_DIRS`.
- Sources are listed in `main/CMakeLists.txt` (section `SRCS`).
