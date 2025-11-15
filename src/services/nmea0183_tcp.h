#ifndef NMEA0183_TCP_H
#define NMEA0183_TCP_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

/**
 * NMEA 0183 TCP Server
 *
 * Broadcasts NMEA 0183 sentences over TCP on port 10110
 * Supports multiple simultaneous clients
 */

// TCP Server port (standard NMEA 0183 TCP port)
#define NMEA_TCP_PORT 10110

// Maximum number of simultaneous clients
#define MAX_NMEA_CLIENTS 8

// Client timeout in milliseconds
#define CLIENT_TIMEOUT_MS 30000

// Buffer size per client
#define CLIENT_BUFFER_SIZE 512

/**
 * Initialize NMEA 0183 TCP server
 * Starts listening on port 10110
 */
void initNMEA0183Server();

/**
 * Process TCP server - accept new clients and handle disconnections
 * Call this in the main loop
 */
void processNMEA0183Server();

/**
 * Broadcast NMEA 0183 sentence to all connected clients
 *
 * @param sentence NMEA 0183 sentence (must include checksum and CRLF)
 */
void broadcastNMEA0183(const String& sentence);

/**
 * Get number of connected clients
 *
 * @return Number of active TCP clients
 */
int getNMEA0183ClientCount();

/**
 * Stop NMEA 0183 TCP server and disconnect all clients
 */
void stopNMEA0183Server();

#endif // NMEA0183_TCP_H
