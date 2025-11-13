#ifndef CONFIG_H
#define CONFIG_H

// NMEA Configuration
#define NMEA_RX 16
#define NMEA_TX 17
#define NMEA_BAUD 4800

// GPS Configuration
#define GPS_BAUD 9600
#define GPS_RX 16
#define GPS_TX 17

// WebSocket Configuration
#define WS_DELTA_MIN_MS 100        // Minimum delta broadcast interval
#define WS_CLEANUP_MS 5000         // WebSocket cleanup interval
#define AUTH_TOKEN_LENGTH 32       // Token length

// TCP Configuration
#define TCP_RECONNECT_DELAY 5000   // TCP reconnect delay in milliseconds

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
