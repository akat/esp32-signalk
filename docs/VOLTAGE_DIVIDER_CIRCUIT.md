# Voltage Divider Circuit for Single-Ended NMEA

## Why Do We Need This?

Marine NMEA 0183 devices typically output signals at **0-12V levels**, but the ESP32 GPIO pins are **only 3.3V tolerant**. Connecting a 12V signal directly to an ESP32 pin will **permanently damage the chip**.

A **voltage divider circuit** safely reduces the 12V signal to approximately 3.3V, protecting your ESP32.

## Circuit Diagram (Detailed)

```
┌──────────────────────────────────────────────────────────────┐
│  NMEA Device (e.g., NASA Wind)                              │
│                                                               │
│  ┌────────────┐                                              │
│  │   Output   │  Typically outputs 0-12V                     │
│  │   Driver   │  with 220Ω source impedance                  │
│  └─────┬──────┘                                              │
│        │                                                      │
└────────┼──────────────────────────────────────────────────────┘
         │ NMEA OUT (0-12V)
         │
         ├───────────────┐
         │               │ +12V when logic HIGH
         │               │  0V when logic LOW
         │               │
        ┌┴┐             ┌┴┐
    R1  │ │ 10kΩ    C1  ─┬─ 0.1µF (optional, for noise filtering)
        │ │  1/4W       ─┴─
        └┬┘              │
         │               │
         ├───────────────┴────→ To ESP32 GPIO 33 (3.0-3.3V)
         │
        ┌┴┐
    R2  │ │ 3.9kΩ
        │ │  1/4W
        └┬┘
         │
        GND ←─────────────────→ Common GND (ESP32 + NMEA Device)


Voltage Division Formula:
V_out = V_in × (R2 / (R1 + R2))
V_out = 12V × (3.9kΩ / 13.9kΩ) = 3.36V ✓
```

## Component Selection

### Resistors

**R1: 10kΩ (Top Resistor)**
- **Value:** 10,000 Ω (10kΩ)
- **Tolerance:** ±5% or ±1% (1% preferred for accuracy)
- **Power Rating:** 1/4W (0.25W) minimum
- **Type:** Carbon film or metal film
- **Common codes:**
  - Color bands: Brown-Black-Orange-Gold (5%)
  - SMD marking: 103

**R2: 3.9kΩ (Bottom Resistor)**
- **Value:** 3,900 Ω (3.9kΩ)
- **Tolerance:** ±5% or ±1% (1% preferred for accuracy)
- **Power Rating:** 1/4W (0.25W) minimum
- **Type:** Carbon film or metal film
- **Common codes:**
  - Color bands: Orange-White-Red-Gold (5%)
  - SMD marking: 392

### Optional: Noise Filtering Capacitor

**C1: 0.1µF Ceramic Capacitor**
- **Value:** 0.1µF (100nF)
- **Type:** Ceramic (X7R or C0G)
- **Voltage:** 16V minimum (25V or 50V recommended)
- **Purpose:** Filters high-frequency noise from NMEA signal

## Alternative Resistor Values

If you can't find 3.9kΩ, here are safe alternatives:

| R1 (kΩ) | R2 (kΩ) | Output Voltage | Status |
|---------|---------|----------------|--------|
| 10      | 3.9     | 3.36V          | ✓ Recommended |
| 10      | 3.3     | 2.97V          | ✓ Safe (slightly low) |
| 22      | 8.2     | 3.26V          | ✓ Good |
| 10      | 4.7     | 3.84V          | ⚠️ High (use with caution) |
| 15      | 5.6     | 3.27V          | ✓ Good |

**Formula to calculate:**
```
V_out = 12V × (R2 / (R1 + R2))
```

**Target range:** 2.8V - 3.3V (safe for ESP32)

## Physical Construction Methods

### Method 1: Breadboard Prototype

Good for testing:

```
     NMEA OUT
         │
         ├────[10kΩ]─────┬─── To GPIO 33
         │               │
        GND    [3.9kΩ]───┴─── To GND
```

**Steps:**
1. Insert 10kΩ resistor from NMEA wire to GPIO 33
2. Insert 3.9kΩ resistor from GPIO 33 to GND
3. Connect NMEA GND to ESP32 GND
4. Optional: Add 0.1µF capacitor across R2

### Method 2: Perfboard/Stripboard

For permanent installations:

**Layout:**
```
  ○ ─── NMEA OUT
  │
 [R1]
  │
  ├─○ ─── GPIO 33    [C1] ─┐ (optional)
  │                         │
 [R2]                       │
  │                         │
  ○ ─────────────────────── GND
```

### Method 3: PCB (Most Reliable)

Create a small PCB with:
- Screw terminals for NMEA input
- Through-hole or SMD resistors
- Header pins for ESP32 connection

### Method 4: Heat-Shrink Inline

For compact installations:

1. Solder R1 (10kΩ) inline with NMEA OUT wire
2. Solder junction to GPIO 33 wire
3. Solder R2 (3.9kΩ) from junction to GND
4. Cover each solder joint with heat-shrink tubing
5. Bundle all connections with larger heat-shrink

## Step-by-Step Build Guide

### Tools Required:
- Soldering iron (25-40W)
- Solder (60/40 or lead-free)
- Wire strippers
- Multimeter (for testing)
- Heat-shrink tubing (optional)

### Steps:

**1. Prepare Components**
```
Cut wires:
- NMEA OUT wire: 15cm
- GPIO 33 wire: 15cm
- Two GND wires: 10cm each
```

**2. Solder R1 (10kΩ)**
```
NMEA OUT ───[10kΩ resistor]─── Junction Point
```

**3. Solder Junction to GPIO 33**
```
Junction Point ────[wire]──── GPIO 33 (ESP32)
```

**4. Solder R2 (3.9kΩ) to GND**
```
Junction Point ───[3.9kΩ resistor]─── GND
```

**5. Connect Device GND to ESP32 GND**
```
NMEA Device GND ───[wire]──── ESP32 GND
```

**6. Optional: Add Capacitor**
```
Place 0.1µF capacitor between GPIO 33 and GND
(parallel to R2)
```

**7. Insulate Connections**
- Cover all solder joints with heat-shrink tubing
- Or use electrical tape
- Ensure no exposed conductors

## Testing the Circuit

### Before Connecting to ESP32:

**1. Visual Inspection**
- ✓ R1 is between NMEA OUT and junction
- ✓ R2 is between junction and GND
- ✓ No shorts between wires
- ✓ All solder joints are solid

**2. Resistance Check (Multimeter)**
```
Measure resistance between:
- NMEA OUT and GND: Should be ~13.9kΩ (R1 + R2)
- Junction and GND: Should be ~3.9kΩ (R2)
```

**3. Voltage Test (With NMEA Device Powered)**
```
Set multimeter to DC voltage (20V range)
- Measure between NMEA OUT and GND: Should show 0-12V
- Measure between junction (GPIO 33) and GND: Should show 0-3.3V

⚠️ If voltage at GPIO 33 exceeds 3.5V, DO NOT CONNECT TO ESP32!
   Check resistor values and connections.
```

## Protection Features (Advanced)

### Add Zener Diode Clamp

For extra protection, add a 3.3V Zener diode:

```
     NMEA OUT
         │
         ├────[10kΩ]─────┬─── To GPIO 33
         │               │
                    ┌────┴────┐
                    │  Zener  │ 3.3V (BZX55C3V3)
                    │  Diode  │
                    └────┬────┘
                         │
        GND   [3.9kΩ]────┴─── To GND
```

**Zener diode** clamps voltage to max 3.3V even if voltage divider fails.

### Add Series Protection Resistor

For additional current limiting:

```
     NMEA OUT
         │
         ├────[10kΩ]─────┬───[100Ω]─── To GPIO 33
         │               │
        GND    [3.9kΩ]───┴─── To GND
```

## Common Mistakes

### ❌ **WRONG: No voltage divider**
```
NMEA OUT (12V) ───────→ GPIO 33  ⚠️ WILL DAMAGE ESP32!
```

### ❌ **WRONG: Reversed resistor values**
```
NMEA OUT ─[3.9kΩ]─┬─ GPIO 33
                   │
                [10kΩ]
                   │
                  GND

Result: V_out = 12V × (10kΩ/13.9kΩ) = 8.6V  ⚠️ TOO HIGH!
```

### ✓ **CORRECT: Proper voltage divider**
```
NMEA OUT ─[10kΩ]─┬─ GPIO 33
                  │
               [3.9kΩ]
                  │
                 GND

Result: V_out = 12V × (3.9kΩ/13.9kΩ) = 3.36V  ✓
```

## Troubleshooting

### Problem: No signal received

**Check:**
1. Continuity between NMEA OUT and GPIO 33 (through R1)
2. R2 is connected to GND
3. NMEA device GND is connected to ESP32 GND

### Problem: Weak/intermittent signal

**Possible causes:**
1. Cold solder joints - Reflow connections
2. Wrong resistor values - Verify with multimeter
3. Broken wire - Check continuity

### Problem: ESP32 damaged/not working

**If voltage divider failed:**
1. **STOP** - Disconnect power immediately
2. Check resistor values with multimeter
3. Verify no short circuits
4. Measure voltage at GPIO 33 (should be ≤3.3V)
5. If ESP32 is damaged, GPIO 33 may be permanently destroyed

## Power Dissipation Calculation

Check if resistors can handle the power:

```
Current through divider:
I = V_in / (R1 + R2) = 12V / 13.9kΩ = 0.86mA

Power dissipated in R1:
P_R1 = I² × R1 = (0.86mA)² × 10kΩ = 7.4mW

Power dissipated in R2:
P_R2 = I² × R2 = (0.86mA)² × 3.9kΩ = 2.9mW
```

**Result:** Both resistors dissipate <10mW, well within 1/4W (250mW) rating. ✓

## Purchase Links (Example Suppliers)

**Resistor Kits:**
- Amazon: "1/4W Metal Film Resistor Kit"
- Mouser: 71-CCF07-E3 series
- Digikey: CFR-25JB series

**Capacitors:**
- Amazon: "0.1uF Ceramic Capacitor Kit"
- Mouser: 80-C315C104M5U
- Digikey: 399-4151-ND

**Complete Kit Option:**
- Search for "ESP32 voltage divider kit 12V to 3.3V"

## Summary Checklist

Before connecting to ESP32:

- [ ] R1 = 10kΩ installed between NMEA OUT and GPIO 33
- [ ] R2 = 3.9kΩ installed between GPIO 33 and GND
- [ ] All solder joints are solid and insulated
- [ ] NMEA device GND connected to ESP32 GND
- [ ] Resistance measured: ~13.9kΩ total, ~3.9kΩ to GND
- [ ] Voltage tested: ≤3.3V at GPIO 33 junction
- [ ] No short circuits between wires
- [ ] Optional capacitor installed (if desired)

## References

- **Voltage Divider Calculator:** https://ohmslawcalculator.com/voltage-divider-calculator
- **Resistor Color Code:** https://www.digikey.com/en/resources/conversion-calculators/conversion-calculator-resistor-color-code
- **ESP32 Datasheet:** https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf

---

**Safety First:** Always test the voltage divider with a multimeter BEFORE connecting to ESP32!

**Document Version:** 1.0
**Last Updated:** 2025-01-21
