#ifndef CONFIG_H
#define CONFIG_H

// NMEA Configuration
#define NMEA_RX 16
#define NMEA_TX 17
#define NMEA_BAUD 4800

// WebSocket Configuration
#define WS_DELTA_MIN_MS 100        // Minimum delta broadcast interval
#define WS_CLEANUP_MS 5000         // WebSocket cleanup interval
#define AUTH_TOKEN_LENGTH 32       // Token length

#endif
