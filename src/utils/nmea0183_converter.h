#ifndef NMEA0183_CONVERTER_H
#define NMEA0183_CONVERTER_H

#include <Arduino.h>

/**
 * NMEA 0183 Converter Utility
 * Converts NMEA 2000 PGN data to NMEA 0183 sentences
 */

// Calculate NMEA 0183 checksum (XOR of all chars between $ and *)
String calculateNMEAChecksum(const String& sentence);

// Add checksum and CRLF to sentence
String addNMEAChecksum(const String& sentence);

// Helper formatting functions
char* formatLatitude(double latitude, char* buffer);
char* formatLongitude(double longitude, char* buffer);
char* formatTime(const String& timestamp, char* buffer);
char* formatDate(const String& timestamp, char* buffer);

// NMEA 2000 to NMEA 0183 converters
String convertToGGA(double latitude, double longitude, const String& timestamp, int satellites = 0, double altitude = NAN);
String convertToGLL(double latitude, double longitude, const String& timestamp);
String convertToVTG(double cogRad, double sogMS);
String convertToRMC(double latitude, double longitude, double cogRad, double sogMS, const String& timestamp);
String convertToMWV(double angleRad, double speedMS, char reference = 'T');
String convertToDPT(double depthM, double offsetM = 0.0);
String convertToMTW(double tempK);
String convertToHDG(double headingRad, bool magnetic = false);

#endif // NMEA0183_CONVERTER_H
