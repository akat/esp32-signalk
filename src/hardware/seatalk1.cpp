#include "seatalk1.h"
#include "../signalk/data_store.h"
#include "../config.h"

#ifdef SEATALK1_USE_SOFTSERIAL
  #include <SoftwareSerial.h>
  static SoftwareSerial* seatalkSerial = nullptr;
#else
  #include <HardwareSerial.h>
  static HardwareSerial* seatalkSerial = nullptr;
#endif

static bool seatalk1Enabled = false;
static bool debugEnabled = false;

// Message parsing state
static uint8_t msgBuffer[SEATALK_MAX_MSG_LEN];
static uint8_t msgIndex = 0;
static uint8_t expectedLength = 0;
static bool inMessage = false;
static unsigned long lastByteTime = 0;

// Statistics
static uint32_t messagesReceived = 0;
static uint32_t messagesDecoded = 0;
static uint32_t parityErrors = 0;

/**
 * Initialize Seatalk 1 interface
 */
bool initSeatalk1(uint8_t rxPin) {
  Serial.println("\n=== Initializing Seatalk 1 ===");

#ifdef SEATALK1_USE_SOFTSERIAL
  // Use SoftwareSerial - no hardware serial conflicts!
  Serial.println("Mode: SoftwareSerial (no RS485/GPS conflicts)");

  // Create SoftwareSerial instance (RX only, no TX needed for Seatalk)
  seatalkSerial = new SoftwareSerial(rxPin, -1, true);  // inverted = true

  if (!seatalkSerial) {
    Serial.println("ERROR: Failed to create SoftwareSerial instance");
    return false;
  }

  // Begin SoftwareSerial at Seatalk baud rate
  seatalkSerial->begin(SEATALK_BAUD);

  Serial.println("SoftwareSerial initialized successfully");
#else
  // Fallback: Use HardwareSerial (legacy mode)
  Serial.println("Mode: HardwareSerial (may conflict with RS485)");
  Serial.println("WARNING: Consider enabling SEATALK1_USE_SOFTSERIAL in config.h");

  seatalkSerial = &Serial1;
  seatalkSerial->begin(SEATALK_BAUD, SERIAL_8N1, rxPin, -1, true);  // inverted = true
#endif

  seatalk1Enabled = true;

  Serial.printf("Seatalk RX Pin: GPIO %d\n", rxPin);
  Serial.printf("Baud Rate: %d\n", SEATALK_BAUD);
  Serial.println("Signal: Inverted (12V bus via level shifter)");
  Serial.println("\n*** HARDWARE REQUIREMENTS ***");
  Serial.println("1. Opto-isolated level shifter (12V → 3.3V)");
  Serial.println("2. Inverted logic (handled by SoftwareSerial)");
  Serial.println("3. Seatalk wiring:");
  Serial.println("   - Yellow wire → Level shifter input");
  Serial.println("   - Red wire → +12V (keep isolated!)");
  Serial.println("   - Black/Shield → Ground (common with ESP32)");
  Serial.println("\n*** WARNING ***");
  Serial.println("Never connect Seatalk directly to ESP32!");
  Serial.println("12V will damage the GPIO pins.");
  Serial.println("Always use proper level shifting/isolation.");
  Serial.println("================================\n");

  return true;
}

/**
 * Process incoming Seatalk data
 */
bool processSeatalk1() {
  if (!seatalk1Enabled || !seatalkSerial) {
    return false;
  }

  unsigned long now = millis();
  bool messageProcessed = false;

  // Timeout detection - reset if no data for 100ms
  if (inMessage && (now - lastByteTime > 100)) {
    if (debugEnabled) {
      Serial.println("Seatalk: Message timeout, resetting");
    }
    inMessage = false;
    msgIndex = 0;
    expectedLength = 0;
  }

  while (seatalkSerial->available()) {
    // Check for parity/framing errors (indicates command byte)
    bool isCommandByte = false;

    // On ESP32, we can check UART status register for parity errors
    // This is a workaround since ESP32 doesn't support true 9-bit mode
    // The parity error indicates the 9th bit was set (command byte)

    uint8_t byte = seatalkSerial->read();
    lastByteTime = now;

    // Simple heuristic: After a gap or at start, assume command byte
    if (!inMessage || (now - lastByteTime > 50)) {
      isCommandByte = true;
    }

    if (isCommandByte) {
      // Start of new message
      msgBuffer[0] = byte;  // Command byte
      msgIndex = 1;
      inMessage = true;
      expectedLength = 0;  // Will be determined from next byte

      if (debugEnabled) {
        Serial.printf("Seatalk CMD: 0x%02X\n", byte);
      }
    } else if (inMessage) {
      if (msgIndex < SEATALK_MAX_MSG_LEN) {
        msgBuffer[msgIndex++] = byte;

        // Second byte contains length information
        if (msgIndex == 2) {
          // Length is encoded in lower 4 bits of attribute byte
          uint8_t attr = msgBuffer[1];
          expectedLength = 3 + (attr & 0x0F);  // Command + Attr + (attr&0x0F) data bytes

          if (debugEnabled) {
            Serial.printf("Seatalk ATTR: 0x%02X, Expected Length: %d\n", attr, expectedLength);
          }
        }

        // Check if we have complete message
        if (expectedLength > 0 && msgIndex >= expectedLength) {
          // Process complete message
          SeatalkMessage msg;
          msg.command = msgBuffer[0];
          msg.attribute = msgBuffer[1];
          msg.length = msgIndex;
          msg.valid = true;

          for (uint8_t i = 2; i < msgIndex && i < 18; i++) {
            msg.data[i - 2] = msgBuffer[i];
          }

          messagesReceived++;
          decodeSeatalkMessage(msg);
          messageProcessed = true;

          // Reset for next message
          inMessage = false;
          msgIndex = 0;
          expectedLength = 0;
        }
      } else {
        // Buffer overflow - reset
        if (debugEnabled) {
          Serial.println("Seatalk: Buffer overflow");
        }
        inMessage = false;
        msgIndex = 0;
        expectedLength = 0;
      }
    }
  }

  return messageProcessed;
}

/**
 * Decode Seatalk message and update SignalK data
 */
void decodeSeatalkMessage(const SeatalkMessage& msg) {
  if (!msg.valid) return;

  if (debugEnabled) {
    Serial.printf("Seatalk [0x%02X]: ", msg.command);
    Serial.printf("%02X ", msg.attribute);
    for (uint8_t i = 0; i < msg.length - 2; i++) {
      Serial.printf("%02X ", msg.data[i]);
    }
    Serial.println();
  }

  messagesDecoded++;

  switch (msg.command) {
    case ST_DEPTH_BELOW_TRANSDUCER: {  // 00 02 YZ XX XX
      if (msg.length >= 5) {
        // Depth in feet = (XX XX + YZ*256) / 10
        uint16_t depthRaw = msg.data[1] | (msg.data[2] << 8);
        uint8_t yz = msg.attribute;
        float depthFeet = (depthRaw + (yz & 0x0F) * 256) / 10.0;
        float depthMeters = depthFeet * 0.3048;  // Convert feet to meters

        setPathValue("environment.depth.belowTransducer", depthMeters, "seatalk1");

        if (debugEnabled) {
          Serial.printf("Depth: %.2f m (%.1f ft)\n", depthMeters, depthFeet);
        }
      }
      break;
    }

    case ST_APPARENT_WIND_ANGLE: {  // 10 01 XX YY
      if (msg.length >= 4) {
        // Wind angle in degrees = XX * 2
        // YY bits indicate port/starboard
        uint8_t angleRaw = msg.data[0];
        uint8_t direction = msg.data[1];

        float angle = angleRaw * 2.0;
        if (direction & 0x80) {
          angle = 360.0 - angle;  // Port side
        }

        // Convert to radians for SignalK
        float angleRad = angle * PI / 180.0;
        setPathValue("environment.wind.angleApparent", angleRad, "seatalk1");

        if (debugEnabled) {
          Serial.printf("Apparent Wind Angle: %.1f° (%s)\n",
                       angle, (direction & 0x80) ? "Port" : "Starboard");
        }
      }
      break;
    }

    case ST_APPARENT_WIND_SPEED: {  // 11 01 XX 0Y
      if (msg.length >= 4) {
        // Wind speed in knots = XX + Y/10
        uint8_t knots = msg.data[0];
        uint8_t decimal = msg.data[1] & 0x0F;

        float speedKnots = knots + decimal / 10.0;
        float speedMs = speedKnots * 0.514444;  // Convert knots to m/s

        setPathValue("environment.wind.speedApparent", speedMs, "seatalk1");

        if (debugEnabled) {
          Serial.printf("Apparent Wind Speed: %.1f kn (%.2f m/s)\n", speedKnots, speedMs);
        }
      }
      break;
    }

    case ST_SPEED_THROUGH_WATER: {  // 20 01 XX XX
      if (msg.length >= 4) {
        // Speed in knots = XX XX / 10
        uint16_t speedRaw = msg.data[0] | (msg.data[1] << 8);
        float speedKnots = speedRaw / 10.0;
        float speedMs = speedKnots * 0.514444;  // Convert to m/s

        setPathValue("navigation.speedThroughWater", speedMs, "seatalk1");

        if (debugEnabled) {
          Serial.printf("Speed Through Water: %.1f kn (%.2f m/s)\n", speedKnots, speedMs);
        }
      }
      break;
    }

    case ST_WATER_TEMPERATURE: {  // 23 Z1 XX YY
      if (msg.length >= 4) {
        // Temperature in Celsius = (XX YY - 100) / 10
        uint16_t tempRaw = msg.data[0] | ((msg.data[1] & 0x0F) << 8);
        float tempC = (tempRaw - 100) / 10.0;
        float tempK = tempC + 273.15;  // Convert to Kelvin for SignalK

        setPathValue("environment.water.temperature", tempK, "seatalk1");

        if (debugEnabled) {
          Serial.printf("Water Temperature: %.1f°C (%.1f K)\n", tempC, tempK);
        }
      }
      break;
    }

    case ST_COMPASS_HEADING_MAG: {  // 9C U1 VW XY
      if (msg.length >= 4) {
        // Heading in degrees = (VW + U*256) / 2
        uint8_t u = msg.attribute & 0x0F;
        uint16_t headingRaw = msg.data[0] | (u << 8);
        float headingDeg = (headingRaw & 0x0FFF) / 2.0;
        float headingRad = headingDeg * PI / 180.0;

        setPathValue("navigation.headingMagnetic", headingRad, "seatalk1");

        if (debugEnabled) {
          Serial.printf("Magnetic Heading: %.1f° (%.3f rad)\n", headingDeg, headingRad);
        }
      }
      break;
    }

    case ST_COMPASS_HEADING_AUTO: {  // 84 U6 VW XY Z0 0M
      if (msg.length >= 6) {
        // Autopilot course
        uint8_t u = (msg.attribute >> 4) & 0x0F;
        uint16_t courseRaw = msg.data[0] | ((u & 0x03) << 8);
        float courseDeg = courseRaw / 2.0;
        float courseRad = courseDeg * PI / 180.0;

        setPathValue("steering.autopilot.target.headingMagnetic", courseRad, "seatalk1");

        if (debugEnabled) {
          Serial.printf("Autopilot Course: %.1f° (%.3f rad)\n", courseDeg, courseRad);
        }
      }
      break;
    }

    default:
      if (debugEnabled) {
        Serial.printf("Unknown Seatalk command: 0x%02X (%s)\n",
                     msg.command, getSeatalkCommandName(msg.command).c_str());
      }
      break;
  }
}

/**
 * Get human-readable command name
 */
String getSeatalkCommandName(uint8_t command) {
  switch (command) {
    case ST_DEPTH_BELOW_TRANSDUCER: return "Depth Below Transducer";
    case ST_EQUIPMENT_ID: return "Equipment ID";
    case ST_APPARENT_WIND_ANGLE: return "Apparent Wind Angle";
    case ST_APPARENT_WIND_SPEED: return "Apparent Wind Speed";
    case ST_SPEED_THROUGH_WATER: return "Speed Through Water";
    case ST_TRIP_MILEAGE: return "Trip Mileage";
    case ST_TOTAL_MILEAGE: return "Total Mileage";
    case ST_WATER_TEMPERATURE: return "Water Temperature";
    case ST_DISPLAY_UNITS: return "Display Units";
    case ST_TOTAL_TRIP_LOG: return "Total & Trip Log";
    case ST_SPEED_THROUGH_WATER_2: return "Speed Through Water (Alt)";
    case ST_WATER_TEMP_2: return "Water Temperature (Alt)";
    case ST_SET_LAMP_INTENSITY: return "Set Lamp Intensity";
    case ST_WIND_ALARM: return "Wind Alarm";
    case ST_COMPASS_HEADING_AUTO: return "Compass Heading (Autopilot)";
    case ST_NAVIGATION_DATA: return "Navigation Data";
    case ST_KEYSTROKE: return "Keystroke";
    case ST_TARGET_WAYPOINT: return "Target Waypoint";
    case ST_AUTOPILOT_PARAMETER: return "Autopilot Parameter";
    case ST_COMPASS_HEADING_MAG: return "Compass Heading (Magnetic)";
    case ST_EQUIPMENT_ID_2: return "Equipment ID (Alt)";
    default: return "Unknown";
  }
}

/**
 * Check if enabled
 */
bool isSeatalk1Enabled() {
  return seatalk1Enabled;
}

/**
 * Enable/disable debug output
 */
void setSeatalk1Debug(bool enable) {
  debugEnabled = enable;

  if (enable) {
    Serial.println("\n=== Seatalk Debug Enabled ===");
    Serial.printf("Messages Received: %u\n", messagesReceived);
    Serial.printf("Messages Decoded: %u\n", messagesDecoded);
    Serial.printf("Parity Errors: %u\n", parityErrors);
    Serial.println("============================\n");
  }
}
