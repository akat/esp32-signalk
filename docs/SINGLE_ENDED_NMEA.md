# Single-Ended NMEA 0183 Input Guide

## Overview

This ESP32 SignalK server supports **Single-Ended NMEA 0183** devices in addition to RS485 differential NMEA devices. Single-ended NMEA devices output a single-wire signal (typically 0-12V) and are commonly found in:

- **Wind instruments** (NASA Wireless Wind, etc.)
- **Depth sounders** (single-wire output models)
- **GPS modules** (with NMEA 0183 output)
- **Speed logs**
- **Other marine electronics** with single-wire NMEA output

## ⚠️ IMPORTANT: Hardware Requirements

### Voltage Divider Circuit

**CRITICAL:** ESP32 GPIO pins are **3.3V tolerant only**. Most marine NMEA devices output 0-12V signals, which will **damage the ESP32** if connected directly.

You **MUST** build a voltage divider circuit to convert 12V signals to safe 3.3V levels.

### Required Components

- **1x 10kΩ resistor** (R1)
- **1x 3.9kΩ resistor** (R2)
- Optional: **1x 0.1µF capacitor** (for noise filtering)

### Circuit Diagram

```
NMEA Device                ESP32
┌─────────────┐           ┌─────────────┐
│             │           │             │
│  NMEA OUT   ├───┬───────┤  GPIO 33    │
│             │   │       │             │
│     GND     ├───┼───────┤  GND        │
│             │   │       │             │
│   +12V IN   │   │       └─────────────┘
└─────────────┘   │
                  │
                 ┌┴┐ R1 = 10kΩ
                 │ │
                 └┬┘
                  │
                  ├─────── To GPIO 33
                  │
                 ┌┴┐ R2 = 3.9kΩ
                 │ │
                 └┬┘
                  │
                 GND
```

### Wiring Instructions

1. **Connect NMEA OUT** from your device through a **10kΩ resistor** (R1)
2. **Connect GPIO 33** to the junction between R1 and R2
3. **Connect 3.9kΩ resistor** (R2) from GPIO 33 to **GND**
4. **Connect device GND** to **ESP32 GND**
5. **Power the NMEA device** with appropriate voltage (usually 12V)

**Wiring Formula:**
```
NMEA OUT → 10kΩ → GPIO 33 → 3.9kΩ → GND
```

### Voltage Calculation

The voltage divider formula ensures safe voltage levels:

```
V_out = V_in × (R2 / (R1 + R2))
V_out = 12V × (3.9kΩ / (10kΩ + 3.9kΩ))
V_out = 12V × 0.28 = 3.36V ✓ (Safe for ESP32)
```

## Software Configuration

### Option 1: Enable in config.h (Default)

The Single-Ended NMEA input is enabled by default in `src/config.h`:

```cpp
#define USE_SINGLEENDED_NMEA          // Enable single-ended NMEA input
#define SINGLEENDED_NMEA_RX 33        // GPIO 33 (with voltage divider!)
#define SINGLEENDED_NMEA_BAUD 4800    // Common: 4800 baud
```

### Option 2: Configure via Web Interface

1. Connect to ESP32 WiFi or access via network
2. Navigate to **Hardware Settings** page
3. Locate **Single-Ended NMEA 0183 Input** section
4. Configure:
   - **RX Pin**: GPIO 33 (default)
   - **Baud Rate**: Usually 4800 (check your device manual)
5. Click **Save Configuration**
6. **Restart ESP32** for changes to take effect

## Supported GPIO Pins

While GPIO 33 is the default, you can use other input-capable pins:

### Recommended GPIO Pins:
- **GPIO 33** (default, ADC1_CH5)
- **GPIO 32** (ADC1_CH4) - if not used by Seatalk1
- **GPIO 35** (ADC1_CH7, input-only)
- **GPIO 34** (ADC1_CH6, input-only)
- **GPIO 36** (ADC1_CH0, input-only)
- **GPIO 39** (ADC1_CH3, input-only)

### Pins to AVOID:
- GPIO 0, 2, 5, 12, 15 (strapping pins)
- GPIO 6-11 (connected to flash)
- GPIO 16, 17 (PSRAM if enabled)
- GPIO 21, 22 (RS485 TX/RX)
- GPIO 25, 18 (GPS)
- GPIO 26, 27 (CAN bus)

## Common Baud Rates

Most NMEA 0183 devices use standard baud rates:

| Device Type        | Typical Baud Rate |
|-------------------|-------------------|
| **Wind instruments** | 4800 baud         |
| **Depth sounders**   | 4800 baud         |
| **GPS modules**      | 4800 or 9600 baud |
| **Speed logs**       | 4800 baud         |
| **AIS receivers**    | 38400 baud        |

**Check your device manual** for the correct baud rate!

## Supported NMEA Sentences

The system automatically parses all standard NMEA 0183 sentences including:

- **MWV** - Wind Speed and Angle
- **DPT** - Depth
- **DBT** - Depth Below Transducer
- **GGA** - GPS Position
- **RMC** - Recommended Minimum Navigation
- **VTG** - Track and Ground Speed
- **HDG** - Heading
- **And many more...**

All received sentences are automatically:
1. Parsed and stored in SignalK format
2. Broadcast via WebSocket to connected clients
3. Available via REST API
4. Forwarded to NMEA 0183 TCP server (port 10110)

## Testing and Verification

### 1. Check Serial Monitor (115200 baud)

After uploading firmware, open Serial Monitor and look for:

```
=== Single-Ended NMEA 0183 Input ===
IMPORTANT: GPIO 33 requires voltage divider!
Wiring: NMEA OUT → 10kΩ → GPIO 33 → 3.9kΩ → GND
This converts 12V NMEA signal to safe 3.3V

Single-Ended NMEA initialized on GPIO 33 @ 4800 baud (SoftwareSerial)
Waiting for NMEA sentences (wind, depth, etc.)...
====================================
```

### 2. Verify Data Reception

When data is received, you should see:

```
Single-Ended RX: $IIMWV,045,R,12.5,N,A*2E
[Single-Ended NMEA] Received 156 bytes in last 10s
```

### 3. If No Data Received

If you see this warning:

```
[Single-Ended NMEA] ⚠️ NO DATA for 30+ seconds
  Check: Voltage divider wiring, device power, baud rate
```

See the **Troubleshooting** section below.

## Troubleshooting

### Problem: No data received

**Check:**
1. ✓ Voltage divider is correctly built (10kΩ + 3.9kΩ)
2. ✓ NMEA device is powered (usually needs 12V)
3. ✓ Baud rate matches device (check manual)
4. ✓ GPIO 33 connection is secure
5. ✓ GND is connected between device and ESP32
6. ✓ Device is actually transmitting (test with USB-Serial adapter)

### Problem: Corrupted/garbled data

**Possible causes:**
1. **Wrong baud rate** - Try 4800, 9600, or 38400
2. **Poor connections** - Check all wiring
3. **Electrical noise** - Add 0.1µF capacitor between GPIO 33 and GND
4. **Voltage too high/low** - Verify voltage divider values

### Problem: Device stops transmitting after a while

**Possible causes:**
1. **Power issue** - Check device power supply stability
2. **Loose connection** - Verify all connections
3. **Device overheating** - Ensure proper ventilation

### Problem: GPIO 33 doesn't work

**Solutions:**
1. **Change pin in config** - Try GPIO 32, 34, 35, 36, or 39
2. **Rebuild voltage divider** for new pin
3. **Update web interface** settings

## Voltage Divider Calculator

Use this online calculator to compute different resistor values:
https://ohmslawcalculator.com/voltage-divider-calculator

**Target:** 12V input → 3.0-3.3V output

**Common resistor combinations:**
- **10kΩ + 3.9kΩ** = 3.36V (recommended)
- **10kΩ + 3.3kΩ** = 2.97V (safe, slightly lower)
- **22kΩ + 8.2kΩ** = 3.26V (alternative)

## Multiple NMEA Devices

If you have multiple single-wire NMEA devices:

### Option 1: Use Different GPIO Pins
- Device 1 → GPIO 33 (with voltage divider)
- Device 2 → GPIO 32 (with separate voltage divider)
- **Note:** Each device needs its own voltage divider circuit

### Option 2: Use NMEA Multiplexer
- Combine multiple NMEA sources into one stream
- Single voltage divider required
- Commercial multiplexers available

### Option 3: Use RS485 for Additional Devices
- If devices support RS485 (differential), connect to RS485 port
- No voltage divider needed for RS485

## Differences: Single-Ended vs RS485 NMEA

| Feature | Single-Ended NMEA | RS485 NMEA |
|---------|------------------|------------|
| **Wiring** | 1 wire + GND | 2 wires (A/B) + GND |
| **Voltage** | 0-12V (needs divider) | Differential ±5V (no divider) |
| **Distance** | Short (<5m) | Long (>100m) |
| **GPIO Pin** | GPIO 33 (default) | GPIO 21/22 |
| **Examples** | NASA Wind, basic GPS | Professional depth sounders |
| **Noise immunity** | Low | High |

## Safety Notes

⚠️ **NEVER connect 12V directly to ESP32 GPIO pins!**

⚠️ **Always use voltage divider for marine devices**

⚠️ **Double-check resistor values before connecting**

⚠️ **Test with multimeter**: Measure voltage at GPIO pin should be ≤3.3V

## Example: NASA Wireless Wind Base Unit

The NASA Wireless Wind is a common single-ended NMEA device:

**Specifications:**
- Output: Single-wire NMEA 0183
- Voltage: 0-12V
- Baud: 4800
- Sentences: MWV (Wind Speed and Angle)

**Connection:**
```
NASA Base Unit      ESP32 SignalK
┌──────────────┐   ┌────────────────┐
│ NMEA OUT     ├───┤ 10kΩ → GPIO 33 │
│ GND          ├───┤ GND            │
│ +12V IN      │   │                │
└──────────────┘   └────────────────┘
              └─────┤ 3.9kΩ → GND    │
```

**Configuration:**
- RX Pin: GPIO 33
- Baud Rate: 4800

**Expected output:**
```
$IIMWV,045,R,12.5,N,A*2E  (Relative wind: 45° at 12.5 knots)
```

## Technical Notes

### Why SoftwareSerial?

The ESP32 has 3 hardware UART ports, all currently in use:
- **Serial** (UART0): Debug/programming (115200 baud)
- **Serial1** (UART1): RS485 NMEA (GPIO 21/22)
- **Serial2** (UART2): GPS module (GPIO 25/18)

Therefore, Single-Ended NMEA uses **SoftwareSerial** on GPIO 33.

### Performance Considerations

- **Max baud rate**: 38400 (SoftwareSerial limitation)
- **Buffer size**: 120 characters (sufficient for NMEA)
- **Update rate**: Checked every loop iteration (~1ms)

## API Access

### REST API

Read Single-Ended NMEA data via REST API:

```bash
# Get wind data
curl http://esp32-ip/signalk/v1/api/vessels/self/environment/wind

# Get all data
curl http://esp32-ip/signalk/v1/api/vessels/self
```

### WebSocket

Subscribe to real-time updates:

```javascript
const ws = new WebSocket('ws://esp32-ip/signalk/v1/stream');

ws.onopen = () => {
  ws.send(JSON.stringify({
    context: 'vessels.self',
    subscribe: [{ path: 'environment.wind.*' }]
  }));
};
```

## Hardware Settings API

Programmatically configure Single-Ended NMEA:

```bash
# Get current settings
curl http://esp32-ip/api/settings/hardware

# Update settings
curl -X POST http://esp32-ip/api/settings/hardware \
  -H "Content-Type: application/json" \
  -d '{
    "singleended": {
      "rx": 33,
      "baud": 4800
    }
  }'
```

## References

- **NMEA 0183 Standard**: https://www.nmea.org/
- **SignalK Specification**: https://signalk.org/specification/
- **ESP32 GPIO Matrix**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html
- **Voltage Divider Theory**: https://learn.sparkfun.com/tutorials/voltage-dividers

## Support

If you encounter issues:

1. Check **Serial Monitor** output
2. Review **Troubleshooting** section above
3. Verify **voltage divider circuit**
4. Test device with **USB-Serial adapter**
5. Report issues at: https://github.com/your-repo/issues

---

**Document Version:** 1.0
**Last Updated:** 2025-01-21
**Hardware:** ESP32 (TTGO T-CAN485)
**Firmware:** ESP32 SignalK Server
