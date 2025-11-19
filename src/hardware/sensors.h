#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Adafruit_BME280.h>

/**
 * Sensor Interface Module
 *
 * This module provides functions to initialize and read from various
 * I2C sensors (BME280 for temperature, pressure, and humidity).
 */

// Sensor read interval (milliseconds)
const unsigned long SENSOR_READ_INTERVAL = 2000;

/**
 * Initializes all I2C sensors
 * Attempts to initialize BME280 at addresses 0x76 and 0x77
 * Configures sensor sampling parameters
 */
void initI2CSensors();

/**
 * Reads data from I2C sensors and updates path values
 * Throttled to read at SENSOR_READ_INTERVAL to prevent excessive I2C traffic
 *
 * Reads from BME280:
 * - Temperature (converted to Kelvin)
 * - Pressure (in Pa)
 * - Humidity (as relative humidity fraction 0-1)
 */
void readI2CSensors();

#endif // SENSORS_H
