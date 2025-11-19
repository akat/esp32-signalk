# RS485 Terminal Wiring Guide - TTGO T-CAN485

## 🔴 ΣΗΜΑΝΤΙΚΟ: Σειρά Καλωδίων στην Κλέμα

Το TTGO T-CAN485 έχει **ΔΥΟ** πράσινες κλέμες 3 θέσεων:

```
┌─────────────────────────────────────┐
│   TTGO T-CAN485 Board               │
│                                     │
│  LEFT Terminal (CAN Bus)            │
│  ┌─────────────┐                    │
│  │  H  │  L  │  G  │  ← CAN (ignore)│
│  └─────────────┘                    │
│                                     │
│  RIGHT Terminal (RS485) ⭐          │
│  ┌─────────────┐                    │
│  │  A  │  B  │  G  │  ← RS485 NMEA  │
│  └─────────────┘                    │
│   Pin1  Pin2  Pin3                  │
└─────────────────────────────────────┘
```

## ✅ Σωστή Σύνδεση RS485

### Για Depth Sounder / NMEA Instrument

```
Depth Sounder         TTGO T-CAN485 (RIGHT Terminal)
─────────────────────────────────────────────────────
RS485 A (Data+)   →   Terminal Pin 1 (A)
RS485 B (Data-)   →   Terminal Pin 2 (B)
Ground (optional) →   Terminal Pin 3 (G) [or leave empty]
```

### ⚠️ Αν Δεν Έχεις Data - Δοκίμασε Swap!

Αν δεν βλέπεις data, **swap τα A/B**:

```
Depth Sounder         TTGO T-CAN485 (RIGHT Terminal)
─────────────────────────────────────────────────────
RS485 A (Data+)   →   Terminal Pin 2 (B)  ← SWAP!
RS485 B (Data-)   →   Terminal Pin 1 (A)  ← SWAP!
Ground (optional) →   Terminal Pin 3 (G)
```

**Γιατί;** Κάποιες συσκευές έχουν ανάποδα τα A/B labels!

## 🔍 Έλεγχος Ενεργοποίησης RS485

### 1. Έλεγξε το Serial Monitor

Άνοιξε το serial monitor:
```bash
platformio device monitor --baud 115200
```

Θα πρέπει να δεις:
```
=== RS485 Configuration ===
NMEA0183 via RS485 started on terminal blocks A/B
Using built-in RS485 transceiver (GPIO 21/22)
Baud rate: 4800 (Common: 4800 or 9600)
DE pin (GPIO 17): LOW (Receive mode)
DE_ENABLE pin (GPIO 19): LOW (Chip enabled)

DEPTH SOUNDER WIRING:
  Terminal A (Blue)   -> RS485 Data+
  Terminal GND (Black)-> Ground
  Terminal B (White)  -> RS485 Data-

NOTE: If no data received, try:
  1. Swap A/B wires (reversed polarity)
  2. Change baud rate to 9600 in config.h
  3. Check depth sounder power

Waiting for NMEA sentences ($SDDBT, $SDDPT)...
===========================
```

### 2. Έλεγξε το config.h

Άνοιξε το `src/config.h` και δες:

```cpp
#define USE_RS485_FOR_NMEA     // ✅ Πρέπει να είναι uncommented!
#define NMEA_RX 21
#define NMEA_TX 22
#define NMEA_DE 17
#define NMEA_DE_ENABLE 19
#define NMEA_BAUD 4800         // Δοκίμασε: 4800, 9600, 38400
```

### 3. Έλεγξε το Baud Rate

Τα depth sounders χρησιμοποιούν διαφορετικά baud rates:

| Device | Common Baud Rate |
|--------|------------------|
| Most depth sounders | 4800 |
| Some modern units | 9600 |
| High-speed instruments | 38400 |

**Αλλαγή baud rate:**
1. Άνοιξε `src/config.h`
2. Άλλαξε: `#define NMEA_BAUD 9600`
3. Rebuild: `platformio run --target upload`

## 🔧 Troubleshooting Steps

### Βήμα 1: Έλεγξε Power

- ✅ Depth sounder τροφοδοτείται; (LED/display ανοιχτά;)
- ✅ ESP32 τροφοδοτείται; (USB ή 12V;)

### Βήμα 2: Έλεγξε Wiring

```
Physical Check:
1. RIGHT terminal (not LEFT CAN terminal)
2. Wires firmly inserted in terminal
3. Screw terminals tightened
4. Correct wire colors (if labeled)
```

### Βήμα 3: Δοκίμασε Swap A/B

```bash
# Before:
A → Terminal Pin 1
B → Terminal Pin 2

# After (swap):
A → Terminal Pin 2
B → Terminal Pin 1
```

### Βήμα 4: Άλλαξε Baud Rate

Edit `src/config.h`:
```cpp
// Try 9600 instead of 4800
#define NMEA_BAUD 9600
```

Upload:
```bash
platformio run --target upload
```

### Βήμα 5: Έλεγξε Serial Monitor Output

Αν δεις:
```
[RS485 Status] ⚠️ NO DATA for 30+ seconds
  Check: Wiring, Baud rate, Depth sounder power
```

Τότε:
1. ✅ RS485 **ΕΙΝΑΙ ενεργοποιημένο**
2. ⚠️ Πρόβλημα: Wiring ή baud rate ή depth sounder

## 📊 RS485 Status Messages

### ✅ ΚΑΛΟ - Παίρνει Data
```
RS485 RX: $SDDBT,12.3,f,3.7,M,2.0,F*3C
Depth: 3.7 m (12.3 ft)
```

### ⚠️ ΠΡΟΒΛΗΜΑ - Δεν Παίρνει Data
```
[RS485 Status] Received 0 bytes in last 10s
[RS485 Status] ⚠️ NO DATA for 30+ seconds
```

### 🔍 Debugging - Βλέπεις Bytes αλλά όχι NMEA
```
RS485: Non-printable 0xFF ('?')
RS485 Invalid: [garbage] (len=8)
```
→ Wrong baud rate ή inverted polarity

## 🎯 Quick Fix Checklist

- [ ] **RIGHT terminal** (not LEFT CAN terminal)
- [ ] **Pin 1 (A)** = Data+ (usually Blue/White+)
- [ ] **Pin 2 (B)** = Data- (usually White-/Blue-)
- [ ] **Pin 3 (G)** = Ground (optional, usually Black)
- [ ] Depth sounder **powered on**
- [ ] ESP32 shows "RS485 Configuration" in serial
- [ ] Baud rate = 4800 or 9600
- [ ] If no data → **Swap A/B wires**
- [ ] If garbage → **Change baud rate**

## 📸 Visual Guide

### Terminal Block Layout (Top View)

```
     USB Port Side
         ↓
┌────────────────────┐
│                    │
│   LEFT Terminal    │  ← CAN Bus (ignore)
│   [H] [L] [G]      │
│                    │
│   RIGHT Terminal   │  ← RS485 NMEA ⭐
│   [A] [B] [G]      │
│    ↑   ↑   ↑       │
│    1   2   3       │
└────────────────────┘
  Terminal Block Side
```

### Wire Colors (Common Standards)

| Color | RS485 Signal | Terminal |
|-------|--------------|----------|
| Blue/White+ | A (Data+) | Pin 1 |
| White-/Blue- | B (Data-) | Pin 2 |
| Black/Brown | Ground | Pin 3 |

**⚠️ NOTE:** Color standards vary! Check your depth sounder manual!

## 🔌 Alternative: GPIO Header Pins

If RS485 terminal doesn't work, try GPIO header:

1. **Disable RS485 mode:**
   ```cpp
   // Comment out in config.h:
   // #define USE_RS485_FOR_NMEA
   ```

2. **Connect to GPIO header:**
   ```
   Depth Sounder TX  →  GPIO 32 (Pin 3)
   Depth Sounder GND →  GND (Pin 2)
   ```

3. **Rebuild & upload**

## 📞 Still No Data?

### Check Depth Sounder:
1. Is it transmitting? (check with another device)
2. Is NMEA output enabled? (some need configuration)
3. Is it set to RS485 mode? (some have RS232/RS485 switch)

### Check ESP32:
1. Serial monitor shows "RS485 Configuration" message?
2. Shows "Waiting for NMEA sentences"?
3. Shows "NO DATA" warnings every 10s?

If YES to all above → Problem is **wiring or baud rate**, not firmware!

## 🎓 Understanding RS485

RS485 uses **differential signaling**:
- **A (Data+)**: Positive data line
- **B (Data-)**: Negative data line (inverted)
- **Signal**: Voltage difference between A and B

**Polarity matters!** If A/B reversed:
- Data is inverted/corrupted
- No valid NMEA sentences decoded

**Solution:** Swap A/B if no data!

---

**Last Updated:** January 2025
**Board:** LILYGO TTGO T-CAN485
**Firmware:** ESP32-SignalK with RS485 support
