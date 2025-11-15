#include "nmea0183_converter.h"
#include <math.h>

// Calculate NMEA 0183 checksum
String calculateNMEAChecksum(const String& sentence) {
  uint8_t checksum = 0;

  // Start after '$', end before '*' (if present)
  int start = (sentence[0] == '$') ? 1 : 0;
  int end = sentence.indexOf('*');
  if (end == -1) end = sentence.length();

  for (int i = start; i < end; i++) {
    checksum ^= sentence[i];
  }

  char hex[3];
  sprintf(hex, "%02X", checksum);
  return String(hex);
}

// Add checksum and CRLF to sentence
String addNMEAChecksum(const String& sentence) {
  String result = sentence;

  // Remove existing checksum if present
  int asterisk = result.indexOf('*');
  if (asterisk != -1) {
    result = result.substring(0, asterisk);
  }

  // Add checksum
  result += "*";
  result += calculateNMEAChecksum(sentence);
  result += "\r\n";

  return result;
}

// Format latitude to NMEA format: DDMM.MMMM,N/S
char* formatLatitude(double latitude, char* buffer) {
  char ns = (latitude >= 0) ? 'N' : 'S';
  latitude = fabs(latitude);

  int degrees = (int)latitude;
  double minutes = (latitude - degrees) * 60.0;

  sprintf(buffer, "%02d%07.4f,%c", degrees, minutes, ns);
  return buffer;
}

// Format longitude to NMEA format: DDDMM.MMMM,E/W
char* formatLongitude(double longitude, char* buffer) {
  char ew = (longitude >= 0) ? 'E' : 'W';
  longitude = fabs(longitude);

  int degrees = (int)longitude;
  double minutes = (longitude - degrees) * 60.0;

  sprintf(buffer, "%03d%07.4f,%c", degrees, minutes, ew);
  return buffer;
}

// Format time from timestamp: HHMMSS.sss
char* formatTime(const String& timestamp, char* buffer) {
  if (timestamp.length() >= 19) {
    // ISO8601: 2024-01-15T12:34:56.789Z
    int hour = timestamp.substring(11, 13).toInt();
    int minute = timestamp.substring(14, 16).toInt();
    int second = timestamp.substring(17, 19).toInt();
    sprintf(buffer, "%02d%02d%02d.000", hour, minute, second);
  } else {
    // Use current time or default
    strcpy(buffer, "000000.000");
  }
  return buffer;
}

// Format date from timestamp: DDMMYY
char* formatDate(const String& timestamp, char* buffer) {
  if (timestamp.length() >= 10) {
    // ISO8601: 2024-01-15T...
    int year = timestamp.substring(2, 4).toInt();
    int month = timestamp.substring(5, 7).toInt();
    int day = timestamp.substring(8, 10).toInt();
    sprintf(buffer, "%02d%02d%02d", day, month, year);
  } else {
    strcpy(buffer, "010101");
  }
  return buffer;
}

// Convert to GGA sentence (Global Positioning System Fix Data)
String convertToGGA(double latitude, double longitude, const String& timestamp,
                    int satellites, double altitude) {
  char lat[16], lon[16], time[16];

  formatLatitude(latitude, lat);
  formatLongitude(longitude, lon);
  formatTime(timestamp, time);

  int quality = (satellites > 0) ? 1 : 0;  // 0=invalid, 1=GPS fix

  char sentence[128];
  if (isnan(altitude)) {
    sprintf(sentence, "$GPGGA,%s,%s,%s,%d,%02d,1.0,,,M,,M,,",
            time, lat, lon, quality, satellites);
  } else {
    sprintf(sentence, "$GPGGA,%s,%s,%s,%d,%02d,1.0,%.1f,M,0.0,M,,",
            time, lat, lon, quality, satellites, altitude);
  }

  return addNMEAChecksum(sentence);
}

// Convert to GLL sentence (Geographic Position - Latitude/Longitude)
String convertToGLL(double latitude, double longitude, const String& timestamp) {
  char lat[16], lon[16], time[16];

  formatLatitude(latitude, lat);
  formatLongitude(longitude, lon);
  formatTime(timestamp, time);

  char sentence[96];
  sprintf(sentence, "$GPGLL,%s,%s,%s,A,A", lat, lon, time);

  return addNMEAChecksum(sentence);
}

// Convert to VTG sentence (Track Made Good and Ground Speed)
String convertToVTG(double cogRad, double sogMS) {
  double cogDeg = cogRad * 180.0 / M_PI;
  double sogKnots = sogMS * 1.94384;  // m/s to knots
  double sogKmh = sogMS * 3.6;        // m/s to km/h

  char sentence[96];
  sprintf(sentence, "$GPVTG,%.1f,T,,M,%.2f,N,%.2f,K,A",
          cogDeg, sogKnots, sogKmh);

  return addNMEAChecksum(sentence);
}

// Convert to RMC sentence (Recommended Minimum Navigation Information)
String convertToRMC(double latitude, double longitude, double cogRad,
                    double sogMS, const String& timestamp) {
  char lat[16], lon[16], time[16], date[16];

  formatLatitude(latitude, lat);
  formatLongitude(longitude, lon);
  formatTime(timestamp, time);
  formatDate(timestamp, date);

  double cogDeg = cogRad * 180.0 / M_PI;
  double sogKnots = sogMS * 1.94384;

  char sentence[128];
  sprintf(sentence, "$GPRMC,%s,A,%s,%s,%.2f,%.1f,%s,,,A",
          time, lat, lon, sogKnots, cogDeg, date);

  return addNMEAChecksum(sentence);
}

// Convert to MWV sentence (Wind Speed and Angle)
String convertToMWV(double angleRad, double speedMS, char reference) {
  double angleDeg = angleRad * 180.0 / M_PI;
  if (angleDeg < 0) angleDeg += 360.0;

  char sentence[64];
  sprintf(sentence, "$WIMWV,%.1f,%c,%.2f,M,A",
          angleDeg, reference, speedMS);

  return addNMEAChecksum(sentence);
}

// Convert to DPT sentence (Depth)
String convertToDPT(double depthM, double offsetM) {
  char sentence[64];
  sprintf(sentence, "$SDDPT,%.2f,%.2f",
          depthM, offsetM);

  return addNMEAChecksum(sentence);
}

// Convert to MTW sentence (Water Temperature)
String convertToMTW(double tempK) {
  double tempC = tempK - 273.15;

  char sentence[64];
  sprintf(sentence, "$YXMTW,%.1f,C",
          tempC);

  return addNMEAChecksum(sentence);
}

// Convert to HDG/HDT sentence (Heading)
String convertToHDG(double headingRad, bool magnetic) {
  double headingDeg = headingRad * 180.0 / M_PI;
  if (headingDeg < 0) headingDeg += 360.0;

  char sentence[64];
  if (magnetic) {
    sprintf(sentence, "$GPHDG,%.1f,,,,",
            headingDeg);
  } else {
    sprintf(sentence, "$GPHDT,%.1f,T",
            headingDeg);
  }

  return addNMEAChecksum(sentence);
}
