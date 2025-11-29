#ifndef NMEA0183_H
#define NMEA0183_H

#include <Arduino.h>
#include <vector>

/**
 * NMEA 0183 Parsing Module
 *
 * This module provides functions to parse NMEA 0183 sentences
 * commonly used in marine navigation systems.
 */

// Forward declarations
struct GPSData;

/**
 * Validates the checksum of an NMEA sentence
 *
 * @param sentence The NMEA sentence to validate (with or without checksum)
 * @return true if checksum is valid or no checksum present, false if invalid
 */
bool validateNmeaChecksum(const String& sentence);

/**
 * Converts NMEA coordinate format to decimal degrees
 *
 * NMEA format:
 * - Latitude: DDMM.MMMM (2 digits degrees, 2 digits minutes, decimal minutes)
 * - Longitude: DDDMM.MMMM (3 digits degrees, 2 digits minutes, decimal minutes)
 *
 * @param coord The coordinate string in NMEA format
 * @param hemisphere Direction indicator ('N'orth, 'S'outh, 'E'ast, 'W'est)
 * @return Decimal degrees, or NAN if invalid
 */
double nmeaCoordToDec(const String& coord, const String& hemisphere);

/**
 * Splits an NMEA sentence into comma-separated fields
 *
 * @param sentence The NMEA sentence (with or without checksum)
 * @return Vector of field strings
 */
std::vector<String> splitNMEA(const String& sentence);

/**
 * Parses an NMEA 0183 sentence and updates navigation/environmental data
 *
 * Supported sentence types:
 * - RMC: Recommended Minimum Position
 * - GGA: GPS Fix Data
 * - VTG: Track & Speed
 * - HDG/HDM/HDT: Heading
 * - GLL: Geographic Position
 * - MWD/MWV: Wind Data
 * - VDR: Set and Drift (Current)
 * - VHW: Water Speed and Heading
 * - VPW: Speed Parallel to Wind
 * - VWT: Wind Speed and Angle True
 * - WCV: Waypoint Closure Velocity
 * - XTE: Cross-Track Error
 * - ZDA: Time & Date
 * - DBT: Depth of Water
 * - GSV: GPS Satellites in View
 *
 * @param sentence The NMEA sentence to parse (starting with '$')
 * @param source The data source identifier (e.g., "nmea0183.GPS", "nmea0183.tcp")
 */
void parseNMEASentence(const String& sentence, const String& source = "nmea0183.GPS");

#endif // NMEA0183_H
