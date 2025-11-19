#include "sensors.h"
#include <Wire.h>

// External declarations for global variables and functions
extern Adafruit_BME280 bme;
extern bool bmeEnabled;
extern unsigned long lastSensorRead;

// I2C Pin definitions
// TTGO T-CAN485 uses GPIO 27/26 for CAN, so GPIO 21/22 are free for I2C
#define I2C_SDA 21 // I2C Data
#define I2C_SCL 22 // I2C Clock

// Helper functions (declared in main)
extern void setPathValue(const String& path, double value, const String& source,
                        const String& unit, const String& description);

// ====== I2C SENSOR HANDLERS ======

void initI2CSensors() {
  Serial.println("Initializing I2C sensors...");
  Serial.println("I2C pins: SDA=21, SCL=22");

  Wire.begin(I2C_SDA, I2C_SCL);

  // Try to initialize BME280
  if (bme.begin(0x76, &Wire)) {
    Serial.println("BME280 sensor found!");
    bmeEnabled = true;

    // Configure BME280
    bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X1,  // temperature
                    Adafruit_BME280::SAMPLING_X1,  // pressure
                    Adafruit_BME280::SAMPLING_X1,  // humidity
                    Adafruit_BME280::FILTER_OFF);
  } else if (bme.begin(0x77, &Wire)) {
    Serial.println("BME280 sensor found at alternate address!");
    bmeEnabled = true;
  } else {
    Serial.println("No BME280 sensor detected");
    bmeEnabled = false;
  }
}

void readI2CSensors() {
  if (!bmeEnabled) return;

  unsigned long now = millis();
  if (now - lastSensorRead < SENSOR_READ_INTERVAL) return;

  lastSensorRead = now;

  float temp = bme.readTemperature();
  float pressure = bme.readPressure();
  float humidity = bme.readHumidity();

  if (!isnan(temp)) {
    double tempK = temp + 273.15;
    setPathValue("environment.inside.temperature", tempK, "i2c.bme280", "K", "Inside temperature");
  }

  if (!isnan(pressure)) {
    setPathValue("environment.inside.pressure", pressure, "i2c.bme280", "Pa", "Inside pressure");
  }

  if (!isnan(humidity)) {
    double relativeHumidity = humidity / 100.0;
    setPathValue("environment.inside.humidity", relativeHumidity, "i2c.bme280", "", "Inside relative humidity");
  }
}
