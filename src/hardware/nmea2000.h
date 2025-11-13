#ifndef NMEA2000_H
#define NMEA2000_H

#include <Arduino.h>
// Note: NMEA2000_CAN.h is included in main.cpp to define the global NMEA2000 instance
// Forward declaration for use in this header
class tN2kMsg;
#include <N2kMessages.h>

/**
 * NMEA 2000 (CAN Bus) Module
 *
 * This module provides handlers and initialization for NMEA 2000
 * (CAN bus) marine data from devices connected via CAN interface.
 */

/**
 * Handles NMEA 2000 Position data (PGN 129025)
 * Updates latitude and longitude information
 *
 * @param N2kMsg Reference to the NMEA 2000 message
 */
void HandleN2kPosition(const tN2kMsg &N2kMsg);

/**
 * Handles NMEA 2000 Course Over Ground and Speed Over Ground (PGN 129026)
 * Updates COG and SOG navigation data
 *
 * @param N2kMsg Reference to the NMEA 2000 message
 */
void HandleN2kCOGSOG(const tN2kMsg &N2kMsg);

/**
 * Handles NMEA 2000 Wind Speed and Angle (PGN 130306)
 * Updates wind speed and direction data
 *
 * @param N2kMsg Reference to the NMEA 2000 message
 */
void HandleN2kWindSpeed(const tN2kMsg &N2kMsg);

/**
 * Handles NMEA 2000 Water Depth (PGN 128267)
 * Updates water depth below transducer
 *
 * @param N2kMsg Reference to the NMEA 2000 message
 */
void HandleN2kWaterDepth(const tN2kMsg &N2kMsg);

/**
 * Handles NMEA 2000 Outside Environment (PGN 130310)
 * Updates water temperature, air temperature, and atmospheric pressure
 *
 * @param N2kMsg Reference to the NMEA 2000 message
 */
void HandleN2kOutsideEnvironment(const tN2kMsg &N2kMsg);

/**
 * Initializes the NMEA 2000 (CAN bus) interface
 * Sets product/device information, message handlers, and mode
 */
void initNMEA2000();

#endif // NMEA2000_H
