#ifndef SEATALK1_H
#define SEATALK1_H

#include <Arduino.h>

/**
 * Seatalk 1 Protocol Handler
 *
 * Seatalk 1 is Raymarine's proprietary protocol used in older marine instruments.
 *
 * Protocol characteristics:
 * - 4800 baud, inverted serial
 * - 9-bit protocol (8 data + 1 command bit)
 * - Single-wire multi-drop bus
 * - 12V signaling (requires level shifter to 3.3V)
 *
 * Hardware Requirements:
 * - Opto-isolated level shifter (12V → 3.3V)
 * - Inverted logic converter
 * - Seatalk Yellow wire = Data
 * - Seatalk Red wire = +12V
 * - Seatalk Black/Shield = Ground
 *
 * Implementation Notes:
 * - ESP32 UART doesn't natively support 9-bit mode
 * - We use Mark/Space parity detection to read the command bit
 * - Messages start with a command byte (9th bit = 1)
 * - Data bytes follow (9th bit = 0)
 */

// Seatalk configuration
#define SEATALK_BAUD 4800
#define SEATALK_MAX_MSG_LEN 18  // Maximum Seatalk message length

// Seatalk message structure
struct SeatalkMessage {
  uint8_t command;        // Command byte (first byte with 9th bit set)
  uint8_t attribute;      // Attribute/length byte
  uint8_t data[16];       // Data bytes
  uint8_t length;         // Total message length (including command)
  bool valid;             // Message validity flag
};

/**
 * Initialize Seatalk 1 interface
 *
 * @param rxPin GPIO pin connected to Seatalk data (via level shifter)
 * @return true if initialization successful
 *
 * Note: Now uses SoftwareSerial (no hardware serial conflicts)
 */
bool initSeatalk1(uint8_t rxPin);

/**
 * Process incoming Seatalk data
 * Should be called frequently from main loop
 *
 * @return true if a complete message was processed
 */
bool processSeatalk1();

/**
 * Decode a Seatalk message and update SignalK data store
 *
 * @param msg The Seatalk message to decode
 */
void decodeSeatalkMessage(const SeatalkMessage& msg);

/**
 * Get human-readable description of a Seatalk command
 *
 * @param command Seatalk command byte
 * @return Description string
 */
String getSeatalkCommandName(uint8_t command);

/**
 * Check if Seatalk interface is enabled
 *
 * @return true if enabled
 */
bool isSeatalk1Enabled();

/**
 * Enable/disable Seatalk debugging output
 *
 * @param enable true to enable debug output
 */
void setSeatalk1Debug(bool enable);

// Common Seatalk datagram types
#define ST_DEPTH_BELOW_TRANSDUCER   0x00  // Depth below transducer
#define ST_EQUIPMENT_ID            0x01  // Equipment ID
#define ST_APPARENT_WIND_ANGLE     0x10  // Apparent wind angle
#define ST_APPARENT_WIND_SPEED     0x11  // Apparent wind speed
#define ST_SPEED_THROUGH_WATER     0x20  // Speed through water
#define ST_TRIP_MILEAGE           0x21  // Trip mileage
#define ST_TOTAL_MILEAGE          0x22  // Total mileage
#define ST_WATER_TEMPERATURE      0x23  // Water temperature
#define ST_DISPLAY_UNITS          0x24  // Display units for mileage/speed
#define ST_TOTAL_TRIP_LOG         0x25  // Total & trip log
#define ST_SPEED_THROUGH_WATER_2  0x26  // Speed through water (alternate)
#define ST_WATER_TEMP_2           0x27  // Water temperature (alternate)
#define ST_SET_LAMP_INTENSITY     0x30  // Set lamp intensity
#define ST_WIND_ALARM             0x36  // Wind alarm
#define ST_COMPASS_HEADING_AUTO   0x84  // Compass heading - autopilot course
#define ST_NAVIGATION_DATA        0x85  // Navigation data
#define ST_KEYSTROKE              0x86  // Keystroke
#define ST_TARGET_WAYPOINT        0x87  // Target waypoint name
#define ST_AUTOPILOT_PARAMETER    0x88  // Autopilot parameter
#define ST_COMPASS_HEADING_MAG    0x9C  // Compass heading - magnetic
#define ST_EQUIPMENT_ID_2         0x90  // Equipment ID (alternate)

#endif // SEATALK1_H
