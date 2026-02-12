# ESPHome Component for the gPlugK

An ESPHome external component for reading encrypted DLMS/COSEM data from Kamstrup Omnipower smart meters, designed for use with the [gPlugK IoT-adapter](https://gplug.ch/produkte/gplugk/).

## Features

- Reads encrypted DLMS/COSEM data via HDLC framing over UART
- AES-128-GCM decryption using mbedTLS (ESP32)
- Exposes energy, power, voltage, current, and power factor as Home Assistant sensors
- Macro-based sensor registration -- only configured sensors are compiled in

## Supported Hardware

- **Meter:** Kamstrup Omnipower
- **Adapter:** [gPlugK](https://gplug.ch/produkte/gplugk/)
- **Platform:** ESP32 (ESP-IDF framework)

## Protocol Stack

```
UART (2400 baud) -> HDLC framing -> DLMS/COSEM -> AES-GCM decrypt -> OBIS decode -> Sensor publish
```

## Requirements

- ESPHome 2024.6.0 or newer
- ESP32 board (e.g. ESP32-C3-DevKitM-1)
- 32-character hex decryption key from your energy provider

## Installation

Reference the component as an external component in your ESPHome YAML config:

```yaml
external_components:
  - source:
      type: local
      path: components/
```

Or from a git repository:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/YOUR_USER/gplugk
      ref: main
    components: [gplugk]
```

## Configuration

### Minimal Example

```yaml
esphome:
  name: esp-gplugk
  friendly_name: "Smart Meter gplugk"

esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: esp-idf

external_components:
  - source:
      type: local
      path: components/

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

logger:
  level: DEBUG

api:

ota:
  platform: esphome

uart:
  rx_pin: GPIO4
  baud_rate: 2400
  rx_buffer_size: 1024

gplugk:
  decryption_key: "00000000000000000000000000000000"  # Replace with your 32-hex-char key
```

### Component Configuration

| Parameter | Required | Description |
|---|---|---|
| `decryption_key` | Yes | 32 hex character string (16 bytes AES key) from your energy provider |

### UART Configuration

| Parameter | Value |
|---|---|
| `baud_rate` | `2400` |
| `rx_pin` | GPIO connected to the gPlugK adapter |
| `rx_buffer_size` | `1024` (recommended) |

## Sensors

All sensors are optional. Only add the ones you need.

### Numeric Sensors (`sensor` platform)

```yaml
sensor:
  - platform: gplugk

    # Energy totals (Wh, total_increasing)
    active_energy_plus:
      name: "Active Energy +"
    active_energy_minus:
      name: "Active Energy -"
    reactive_energy_plus:
      name: "Reactive Energy +"
    reactive_energy_minus:
      name: "Reactive Energy -"

    # Energy per phase (Wh, total_increasing)
    active_energy_plus_l1:
      name: "Active Energy + L1"
    active_energy_plus_l2:
      name: "Active Energy + L2"
    active_energy_plus_l3:
      name: "Active Energy + L3"
    active_energy_minus_l1:
      name: "Active Energy - L1"
    active_energy_minus_l2:
      name: "Active Energy - L2"
    active_energy_minus_l3:
      name: "Active Energy - L3"

    # Meter ID
    meter_id:
      name: "Meter ID"

    # Total power (W)
    active_power_plus:
      name: "Active Power +"
    active_power_minus:
      name: "Active Power -"
    reactive_power_plus:
      name: "Reactive Power +"
    reactive_power_minus:
      name: "Reactive Power -"

    # Active power per phase (W)
    active_power_l1:
      name: "Active Power L1"
    active_power_l2:
      name: "Active Power L2"
    active_power_l3:
      name: "Active Power L3"
    active_power_minus_l1:
      name: "Active Power - L1"
    active_power_minus_l2:
      name: "Active Power - L2"
    active_power_minus_l3:
      name: "Active Power - L3"

    # Voltage per phase (V)
    voltage_l1:
      name: "Voltage L1"
    voltage_l2:
      name: "Voltage L2"
    voltage_l3:
      name: "Voltage L3"

    # Current per phase (A)
    current_l1:
      name: "Current L1"
    current_l2:
      name: "Current L2"
    current_l3:
      name: "Current L3"

    # Power factor
    power_factor:
      name: "Power Factor"
    power_factor_l1:
      name: "Power Factor L1"
    power_factor_l2:
      name: "Power Factor L2"
    power_factor_l3:
      name: "Power Factor L3"
```

### Text Sensors (`text_sensor` platform)

```yaml
text_sensor:
  - platform: gplugk
    timestamp:
      name: "Timestamp"
    meter_name:
      name: "Meter Name"
```

## Build & Deploy

```bash
# Compile firmware
esphome compile esp-gplugk.example.yaml

# Compile and flash to device
esphome run esp-gplugk.example.yaml

# View live logs
esphome logs esp-gplugk.example.yaml
```

## License

MIT License -- see [LICENSE](components/gplugk/LICENSE).
