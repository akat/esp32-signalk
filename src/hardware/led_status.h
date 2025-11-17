#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>

/**
 * LED Status Indicator Manager for TTGO T-CAN485
 *
 * Manages WS2812 RGB LED on GPIO 4:
 * - Red (always on): System is running
 * - Blue (blinking): Internet connectivity status
 *   - Fast blink (500ms): Not connected to internet
 *   - Slow blink (2000ms): Connected to internet
 */

// Initialize WS2812 RGB LED
void initLEDs();

// Update LED state based on connectivity (call from main loop)
void updateLEDs(bool isConnectedToInternet);

#endif // LED_STATUS_H
