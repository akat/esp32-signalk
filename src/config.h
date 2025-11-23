#ifndef CONFIG_H
#define CONFIG_H

// NMEA2000 CAN Bus Configuration (TTGO T-CAN485 board)
// Correct pins for TTGO T-CAN485 hardware
#define CAN_TX_PIN GPIO_NUM_27
#define CAN_RX_PIN GPIO_NUM_26
#define CAN_SE_PIN GPIO_NUM_23  // Silent/Enable pin

// NMEA0183 RS485 Configuration (TTGO T-CAN485 default RS485 pins)
// RS485 accepts DIFFERENTIAL NMEA 0183 devices (2 wires: A and B terminals)
// Examples: depth sounders, differential wind sensors with RS-422 output
#define USE_RS485_FOR_NMEA     // Enable RS485 transceiver for depth sounder
#define NMEA_RX 21
#define NMEA_TX 22
#define NMEA_DE 19             // Direction Enable (DE) pin - TTGO T-CAN485
#define NMEA_DE_ENABLE 17      // Receiver Enable (RE) pin - TTGO T-CAN485
#define NMEA_BAUD 4800         // Common baud rates: 4800, 9600, 38400
                                // Most NMEA 0183 devices use 4800 baud

// Single-Ended NMEA 0183 Input (Direct connection - not RS485)
// For SINGLE-WIRE NMEA 0183 devices (e.g., NASA wind, depth sounders, GPS modules)
// IMPORTANT: Requires voltage divider (12V → 3.3V) - use 10kΩ + 3.9kΩ resistors
// Wiring: NMEA OUT → 10kΩ → GPIO 33 → 3.9kΩ → GND
#define USE_SINGLEENDED_NMEA          // Enable single-ended NMEA input
#define SINGLEENDED_NMEA_RX 33        // GPIO 33 (with voltage divider!)
#define SINGLEENDED_NMEA_BAUD 4800    // Common: 4800 baud (configurable via web UI)

// GPS Configuration
#define GPS_BAUD 9600
#define GPS_RX 25
#define GPS_TX 18

// Seatalk 1 Configuration (Raymarine proprietary protocol)
// IMPORTANT: Requires opto-isolated level shifter (12V → 3.3V)
// Never connect Seatalk directly to ESP32 - will damage GPIO!
#define USE_SEATALK1              // Enable Seatalk 1 (SoftwareSerial - no conflicts!)
#define SEATALK1_RX 32            // GPIO pin for Seatalk RX (via level shifter)
#define SEATALK1_USE_SOFTSERIAL   // Use SoftwareSerial (no conflict with RS485/GPS)
                                   // SoftwareSerial on GPIO 32 = no hardware serial conflict!

// LED Status Indicators (WS2812 RGB LED on TTGO T-CAN485)
#define LED_PIN 4              // WS2812 RGB LED pin
#define LED_COUNT 1            // Single RGB LED on board

// WebSocket Configuration
#define WS_DELTA_MIN_MS 100        // Minimum delta broadcast interval
#define WS_CLEANUP_MS 5000         // WebSocket cleanup interval
#define AUTH_TOKEN_LENGTH 32       // Token length

// TCP Configuration
#define TCP_RECONNECT_DELAY 5000   // TCP reconnect delay in milliseconds

// NMEA 0183 TCP Server Configuration
#define NMEA_TCP_PORT 10110        // Standard NMEA 0183 TCP port
#define MAX_NMEA_CLIENTS 8         // Maximum simultaneous TCP clients
#define CLIENT_TIMEOUT_MS 30000    // TCP client timeout (30 seconds)
#define CLIENT_BUFFER_SIZE 512     // Buffer size per client

// NMEA Rate Limiting
#define NMEA_CLIENT_TIMEOUT 10000  // Client timeout in milliseconds
#define MAX_SENTENCES_PER_SECOND 100 // Maximum NMEA sentences per second

// Access Point Configuration
#define AP_SSID "ESP32-SignalK"
#define AP_PASSWORD "signalk123"
#define AP_CHANNEL 1
#define AP_HIDDEN false
#define AP_MAX_CONNECTIONS 4

#endif
