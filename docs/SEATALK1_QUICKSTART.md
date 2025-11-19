# Seatalk 1 - Quick Start Guide

## ⚡ Quick Setup (5 Minutes)

### 1. Build Level Shifter Circuit

**Minimum Required Circuit (Optocoupler):**

```
Seatalk          Optocoupler PC817        ESP32
Yellow ──[4.7kΩ]──► LED+ ──► Photodiode ──[1kΩ]── +3.3V
                    LED-                  Output ──────── GPIO 32
Ground ─────────────┴────────────────────────────────── GND
```

### 2. Enable in Config

Edit `src/config.h`:
```cpp
#define USE_SEATALK1           // Uncomment this line
#define SEATALK1_RX 32         // GPIO pin
#define SEATALK1_SERIAL 2      // Use Serial2 (if GPS not connected)
```

### 3. Build & Upload

```bash
pio run --target upload
pio device monitor
```

### 4. Verify

Look for this in serial output:
```
=== Seatalk 1 Initialization ===
Seatalk 1 initialized successfully
```

When receiving data:
```
Depth: 5.2 m (17.1 ft)
Apparent Wind Speed: 12.5 kn
```

---

## 🚨 Safety First

1. **TEST** level shifter output with multimeter (must be ≤3.3V)
2. **VERIFY** no 12V reaches ESP32
3. **USE** optocoupler for isolation (recommended)

---

## 🔌 Wiring

| Seatalk | → | Level Shifter | → | ESP32 |
|---------|---|---------------|---|-------|
| Yellow  | → | Input         | → | GPIO 32 (via 3.3V output) |
| Ground  | → | Common GND    | → | GND |
| Red (+12V) | **DO NOT CONNECT** | | |

---

## 📡 Tested Devices

✅ **Working:**
- Raymarine ST40 depth sounder
- Raymarine ST50 wind instrument
- Raymarine ST60+ displays
- Autohelm autopilots

⚠️ **Partial Support:**
- ST80 GPS (requires additional message types)
- Older ST30 instruments (may need signal conditioning)

---

## 🛠️ Troubleshooting

| Problem | Solution |
|---------|----------|
| No data | Check baud rate (4800), verify level shifter works |
| Garbage | Add 100nF capacitor on signal line, check ground |
| ESP32 crashes | **STOP! Check voltage immediately** |
| Only some messages | Normal - add more decoders in `seatalk1.cpp` |

---

## 📖 Full Documentation

See [SEATALK1_HARDWARE.md](SEATALK1_HARDWARE.md) for:
- Complete circuit diagrams
- All supported message types
- Advanced troubleshooting
- Hardware schematics

---

## 💡 Pro Tips

1. **Start with depth sounder** - easiest to test (0x00 messages)
2. **Enable debug mode** - shows all received messages
3. **Use Serial2** - avoids conflicts with RS485 NMEA
4. **Add filtering** - Seatalk is chatty, filter unwanted messages

---

## 🎯 Next Steps

Once working:
1. Access SignalK data at `http://192.168.4.1:3000/signalk/v1/api/`
2. View depth at: `.../vessels/self/environment/depth/belowTransducer`
3. Stream to mobile apps via WebSocket
4. Forward to other NMEA devices via TCP

**Happy Seatalk hacking! 🏴‍☠️**
