/*
  LED Status Indicator Implementation for TTGO T-CAN485

  Manages WS2812 RGB LED on GPIO 4:
  - Red component: Always on - shows system is running
  - Blue component: Blinks based on connectivity
    - Fast blink (500ms): No internet connection
    - Slow blink (2000ms): Connected to internet

  Result: Red + Blue blinking = Purple/Magenta blinking LED
*/

#include "led_status.h"
#include "../config.h"
#include <Adafruit_NeoPixel.h>

// NeoPixel object for WS2812 RGB LED
static Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Internal state tracking
static unsigned long lastStatusBlink = 0;
static bool blueState = false;
static unsigned long currentBlinkInterval = 500;  // Default to fast blink
static bool lastConnectionState = false;

// Color definitions (brightness: 0-255)
#define RED_BRIGHTNESS 50      // Red always on (not too bright)
#define BLUE_BRIGHTNESS 50     // Blue blinks (not too bright)

/**
 * Initialize WS2812 RGB LED
 */
void initLEDs() {
  // Initialize NeoPixel strip
  strip.begin();
  strip.setBrightness(50);  // Set overall brightness (0-255)

  // Set initial color: Red (always on) + Blue off initially
  strip.setPixelColor(0, strip.Color(RED_BRIGHTNESS, 0, 0));
  strip.show();

  blueState = false;
  lastStatusBlink = millis();

  Serial.println("LED Status: WS2812 RGB LED Initialized");
  Serial.printf("  LED Pin: GPIO %d\n", LED_PIN);
  Serial.println("  Red: Always on (system running)");
  Serial.println("  Blue: Blinks (connectivity status)");
}

/**
 * Update LED state based on connectivity
 * @param isConnectedToInternet - true if WiFi is connected and internet is available
 *
 * Call this function regularly from the main loop.
 * It handles the blink timing automatically.
 */
void updateLEDs(bool isConnectedToInternet) {
  unsigned long now = millis();

  // Update blink interval if connection state changed
  if (isConnectedToInternet != lastConnectionState) {
    lastConnectionState = isConnectedToInternet;

    if (isConnectedToInternet) {
      currentBlinkInterval = 2000;  // Slow blink - connected
      Serial.println("LED Status: Connected to internet (blue slow blink - 2s)");
    } else {
      currentBlinkInterval = 500;   // Fast blink - not connected
      Serial.println("LED Status: Not connected to internet (blue fast blink - 500ms)");
    }
  }

  // Handle blue LED blinking (red stays on)
  if (now - lastStatusBlink >= currentBlinkInterval) {
    lastStatusBlink = now;
    blueState = !blueState;

    // Set color: Red (always on) + Blue (blinking)
    uint8_t blue = blueState ? BLUE_BRIGHTNESS : 0;
    strip.setPixelColor(0, strip.Color(RED_BRIGHTNESS, 0, blue));
    strip.show();
  }
}
