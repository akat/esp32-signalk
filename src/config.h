#ifndef CONFIG_H
#define CONFIG_H

// NMEA2000 CAN Bus Configuration (TTGO T-CAN485 board)
// Correct pins for TTGO T-CAN485 hardware
#define CAN_TX_PIN GPIO_NUM_27
#define CAN_RX_PIN GPIO_NUM_26
#define CAN_SE_PIN GPIO_NUM_23  // Silent/Enable pin

// NMEA0183 Configuration (TTGO T-CAN485 default RS485 pins)
#define NMEA_RX 21
#define NMEA_TX 22
#define NMEA_DE 17
#define NMEA_DE_ENABLE 19
#define NMEA_BAUD 4800

// GPS Configuration
#define GPS_BAUD 9600
#define GPS_RX 25
#define GPS_TX 18

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
