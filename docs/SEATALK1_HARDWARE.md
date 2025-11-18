# Seatalk 1 Hardware Setup Guide

## ⚠️ IMPORTANT WARNING

**NEVER connect Seatalk directly to ESP32!**

Seatalk uses 12V signaling which will **permanently damage** ESP32 GPIO pins (which are 3.3V).

You **MUST** use proper level shifting and isolation.

---

## 🔌 Seatalk 1 Protocol Overview

- **Voltage**: 12V (requires level shifter to 3.3V)
- **Baud Rate**: 4800 baud
- **Data Format**: 9-bit (8 data + 1 command bit)
- **Logic**: Inverted serial
- **Topology**: Single-wire multi-drop bus

## 🎨 Seatalk Cable Colors

| Wire Color | Function | Notes |
|------------|----------|-------|
| Yellow | Data | Main signal line |
| Red | +12V | Power (optional, don't connect to ESP32!) |
| Black/Shield | Ground | Common ground with ESP32 |

---

## 🔧 Required Hardware

### Option 1: Opto-Isolated Circuit (RECOMMENDED)

**Safest option with galvanic isolation**

**Components:**
- 1x PC817 or 4N35 optocoupler
- 1x 10kΩ resistor (R1)
- 1x 4.7kΩ resistor (R2)
- 1x 1kΩ resistor (R3)
- 1x 2N3904 or similar NPN transistor

**Circuit Diagram:**

```
Seatalk Yellow (Data)
    |
    +---[R1: 10kΩ]--- Seatalk +12V
    |
    +---[R2: 4.7kΩ]---+---|>|--- Seatalk GND
                      |   LED (PC817 input)
                      |
                    [====]  PC817 Optocoupler
                      |
         +3.3V ---[R3: 1kΩ]---+
                              |
                          Collector (PC817 output)
                              |
                              +---- ESP32 GPIO 32 (SEATALK1_RX)
                              |
                          Emitter
                              |
                             GND (ESP32)
```

**Assembly Steps:**
1. Solder components on a small perfboard
2. Test with multimeter before connecting to ESP32
3. Verify output voltage is 3.3V max

### Option 2: MAX3232 Level Shifter

**Simpler but requires IC**

**Components:**
- 1x MAX3232 or similar RS232 transceiver
- 4x 1µF capacitors
- Voltage divider (10kΩ + 20kΩ resistors)

**Circuit:**

```
Seatalk Yellow → [Voltage Divider 12V→5V] → MAX3232 RX IN → MAX3232 TTL OUT → ESP32 GPIO 32
```

---

## 🔩 Connection to ESP32

### Pin Assignment (Default Configuration)

| ESP32 Pin | Function | Notes |
|-----------|----------|-------|
| GPIO 32 | Seatalk RX | Via level shifter |
| GND | Common Ground | Shared with Seatalk ground |

### Alternative Pins (if GPIO 32 is in use)

Available GPIO pins for Seatalk RX:
- **GPIO 5, 13, 14** (safe, no boot issues)
- **GPIO 33** (safe, has ADC)
- **GPIO 16** (usually free)

**To change the pin:**
Edit [src/config.h](../src/config.h):
```cpp
#define SEATALK1_RX 33  // Change from 32 to 33
```

---

## 📋 Testing & Verification

### Step 1: Test Level Shifter WITHOUT ESP32

1. Connect Seatalk to level shifter
2. Use multimeter to measure output voltage
3. Should see 0V to 3.3V pulses (NOT 12V!)
4. **If you see >3.5V, DO NOT connect to ESP32!**

### Step 2: Connect to ESP32

1. Power off everything
2. Connect level shifter output to GPIO 32
3. Connect common ground
4. Power on ESP32 first, then Seatalk

### Step 3: Enable in Software

Edit [src/config.h](../src/config.h):

```cpp
// Uncomment this line to enable Seatalk 1
#define USE_SEATALK1

// Choose serial port (1 or 2)
#define SEATALK1_SERIAL 1  // Use Serial1
// or
#define SEATALK1_SERIAL 2  // Use Serial2 (if GPS not using it)
```

**Note:** If using Serial1 for Seatalk, you may need to disable RS485 NMEA0183:
```cpp
// Comment out this line to free Serial1
// #define USE_RS485_FOR_NMEA
```

### Step 4: Monitor Serial Output

Upload firmware and monitor serial output:

```bash
pio run --target upload
pio device monitor
```

You should see:
```
=== Seatalk 1 Initialization ===
Seatalk RX Pin: GPIO 32
Baud Rate: 4800
Seatalk 1 initialized successfully
================================
```

When data is received:
```
Seatalk CMD: 0x00
Seatalk ATTR: 0x02, Expected Length: 5
Depth: 5.20 m (17.1 ft)
```

---

## 🛠️ Troubleshooting

### No Data Received

**Possible causes:**
1. **Inverted logic not enabled** - ESP32 UART should be configured with `inverted=true`
2. **Wrong baud rate** - Must be exactly 4800 baud
3. **Level shifter not working** - Test output with oscilloscope
4. **No Seatalk devices transmitting** - Some devices only transmit when active

**Solutions:**
- Check wiring with multimeter
- Verify Seatalk devices are powered and working
- Try swapping Yellow wire polarity (some circuits need inversion)

### Garbage Data / Random Characters

**Possible causes:**
1. **Incorrect baud rate**
2. **Noise on signal line**
3. **Poor grounding**

**Solutions:**
- Add 100nF capacitor between signal line and ground
- Use shielded cable
- Ensure good ground connection

### ESP32 Resets / Crashes

**Possible causes:**
1. **Voltage too high** - Level shifter failed!
2. **ESD damage** - Electrostatic discharge

**Solutions:**
- **Immediately disconnect and test level shifter output**
- Add 100Ω series resistor for protection
- Use TVS diode for ESD protection

---

## 📊 Supported Seatalk Messages

The implementation decodes the following common Seatalk datagrams:

| Code | Description | SignalK Path |
|------|-------------|--------------|
| 0x00 | Depth Below Transducer | `environment.depth.belowTransducer` |
| 0x10 | Apparent Wind Angle | `environment.wind.angleApparent` |
| 0x11 | Apparent Wind Speed | `environment.wind.speedApparent` |
| 0x20 | Speed Through Water | `navigation.speedThroughWater` |
| 0x23 | Water Temperature | `environment.water.temperature` |
| 0x84 | Compass Heading (Autopilot) | `steering.autopilot.target.headingMagnetic` |
| 0x9C | Compass Heading (Magnetic) | `navigation.headingMagnetic` |

More message types can be added in [src/hardware/seatalk1.cpp](../src/hardware/seatalk1.cpp).

---

## 🔗 References

- [Seatalk Protocol Documentation](http://www.thomasknauf.de/seatalk.htm)
- [Seatalk to NMEA Converter Projects](https://github.com/topics/seatalk)
- [ESP32 UART Inverted Mode](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html)

---

## 📝 Schematic Files

You can find example schematics in the `docs/schematics/` folder:
- `seatalk_optocoupler.pdf` - Opto-isolated circuit
- `seatalk_max3232.pdf` - MAX3232 based circuit
- `seatalk_breadboard.png` - Breadboard layout

---

## ✅ Final Checklist

Before connecting to ESP32:

- [ ] Level shifter tested with multimeter (3.3V max output)
- [ ] Common ground connected between Seatalk and ESP32
- [ ] NO direct connection of 12V to ESP32
- [ ] Software enabled in config.h
- [ ] Correct GPIO pin configured
- [ ] Serial port not conflicting with other modules

**Stay safe and happy boating! ⛵**
