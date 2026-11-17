# MQTT IoT Device Monitor

A lightweight, memory-safe command-line utility written in C (C11) that subscribes to IoT device telemetry topics via MQTT, parses and validates incoming JSON payloads, maintains per-device state in memory, detects abnormal sensor thresholds, and publishes JSON alerts back to the broker.

## Project Overview & Architecture

The application is structured into modular components to ensure clean separation of concerns and strict memory management:
- `src/main.c` — CLI argument parsing (`getopt`), asynchronous `libmosquitto` client lifecycle, automatic reconnection handling, threshold evaluation, and graceful shutdown (`SIGINT`/`SIGTERM`).
- `src/device.c` / `src/device.h` — Dynamic registry (`DeviceRegistry`) tracking the latest telemetry state for each unique `device_id`.
- `src/parser.c` / `src/parser.h` — Strict JSON deserialization, field/type validation, and alert JSON serialization using `cJSON`.
- `src/logger.c` / `src/logger.h` — Formatted console output and optional persistent file logging.

## Required Dependencies

### C Application
- `gcc` (with C11 support) and `make`
- `libmosquitto` (`mosquitto-devel` on RHEL/Oracle Linux or `libmosquitto-dev` on Ubuntu/Debian)
- `cJSON` (`cjson-devel` on RHEL/Oracle Linux or `libcjson-dev` on Ubuntu/Debian)
- `mosquitto` MQTT broker and CLI utilities (`mosquitto_pub`, `mosquitto_sub`)

### Python Simulator
- Python 3.8+ with `venv`
- `paho-mqtt >= 2.0`

## Linux & Mosquitto Setup

On Oracle Linux / RHEL 9:

    sudo dnf install epel-release -y
    sudo dnf install gcc make mosquitto mosquitto-devel cjson-devel python3-pip -y

On Ubuntu / Debian:

    sudo apt update
    sudo apt install build-essential mosquitto mosquitto-clients libmosquitto-dev libcjson-dev python3-full python3-venv -y

Starting the Mosquitto Broker (systemd):

    sudo systemctl enable --now mosquitto
    sudo systemctl status mosquitto

Starting the Mosquitto Broker (WSL2 without systemd):

    mosquitto -d

## Build Instructions

The project compiles strictly with `-Wall -Wextra -Werror -std=c11`:

    make clean
    make

This produces the `./iot_monitor` executable in the project root directory.

## Running the Simulator & Application

1. Start the Python Telemetry Simulator:

    ```bash
    python3 -m venv venv
    source venv/bin/activate
    pip install "paho-mqtt>=2.0"
    python mqtt_simulator.py
    ```

2. Start the C Monitoring Application:

    ```bash
    ./iot_monitor -H localhost -P 1883 -o monitor.log
    ```

### Command-Line Options
| Option | Description | Default |
| :--- | :--- | :--- |
| `-H HOST` | MQTT broker hostname or IP address | `localhost` |
| `-P PORT` | MQTT broker port | `1883` |
| `-o FILE` | Optional file path for logging telemetry and alerts | Disabled |
| `-h` | Display help message and exit | — |

## MQTT Topic Structure & Message Formats

### Telemetry Subscription
- **Topic:** `iot/devices/+/telemetry`
- **Payload Format:**

    ```bash
    {"device_id": "sensor_01", "temperature": 24.5, "humidity": 56.2, "battery": 85, "timestamp": 1794301200}
    ```

### Alert Publishing
Alerts are triggered when telemetry exceeds the following thresholds:
- **Temperature > 30.0 C** -> `HIGH_TEMPERATURE`
- **Humidity > 80.0 %** -> `HIGH_HUMIDITY`
- **Battery < 20 %** -> `LOW_BATTERY`

- **Topic:** `iot/devices/<device_id>/alert`
- **Payload Format:**

    ```bash
    {"device_id": "sensor_01", "alert": "LOW_BATTERY", "value": 15, "timestamp": 1794301200}
    ```

## Testing Scenarios & Verification

All required test scenarios were verified locally:

1. **Broker Connection:** Verified successful connection to `localhost:1883` and wildcard subscription to `iot/devices/+/telemetry`.
2. **Multi-Device Tracking:** Confirmed concurrent tracking and state updates for `sensor_01`, `sensor_02`, and `sensor_03`.
3. **JSON Parsing & Display:** Validated readable console formatting (`[HH:MM:SS] Device: ...`) and optional log file output (`-o monitor.log`).
4. **Threshold Detection & Alert Publishing:**
    Monitored alerts in real-time by subscribing to the alert topic:
    ```bash
    mosquitto_sub -h localhost -t 'iot/devices/+/alert' -v
    ```
    Confirmed accurate JSON payloads for `HIGH_TEMPERATURE`, `HIGH_HUMIDITY`, and `LOW_BATTERY`.
5. **Malformed JSON Resilience:** Tested resilience against invalid payloads using `mosquitto_pub` with broken JSON strings and wrong field types; the application logged parsing errors cleanly without crashing.
6. **Broker Disconnection & Reconnection:** Restarted the Mosquitto daemon while `./iot_monitor` was running; the client detected the disconnect, automatically reconnected, and resumed telemetry processing.
7. **Graceful Shutdown & Memory Safety:**
    Terminated the process via `SIGINT` (`Ctrl+C`) under Valgrind to verify memory management:
    ```bash
    valgrind --leak-check=full --show-leak-kinds=all ./iot_monitor
    ```
    Confirmed `0 errors` and `All heap blocks were freed -- no leaks are possible`.

## Known Limitations
- **In-Memory State Persistence:** Device telemetry history is stored in volatile RAM and resets upon application restart.
- **Static Thresholds:** Alert thresholds (`30.0 C`, `80.0 %`, `20 %`) are currently defined as compile-time constants rather than runtime CLI flags or config file entries.
- **Unencrypted MQTT:** Connects over plain TCP (`port 1883`) without TLS/SSL or username/password authentication configured.
