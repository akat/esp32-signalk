#include "nmea0183.h"
#include <cmath>

// External declarations for global variables and functions
extern struct GPSData {
  double lat;
  double lon;
  double sog;
  double cog;
  double heading;
  double altitude;
  int satellites;
  String fixQuality;
  String timestamp;
} gpsData;

// Helper functions (declared in main)
extern String iso8601Now();
extern double knotsToMS(double knots);
extern double degToRad(double deg);
extern void setPathValue(const String& path, double value, const String& source,
                        const String& unit, const String& description);
extern void setPathValue(const String& path, const String& value, const String& source,
                        const String& unit, const String& description);
extern void updateWindAlarm(double windSpeedMS);
extern void updateDepthAlarm(double depth);
extern void updateNavigationPosition(double lat, double lon, const String& source);

// ====== NMEA PARSING ======

bool validateNmeaChecksum(const String& sentence) {
  int starPos = sentence.indexOf('*');
  if (starPos < 0) return true;  // No checksum present, accept sentence

  String data = sentence.substring(1, starPos);  // Skip '$'
  String checksumStr = sentence.substring(starPos + 1);

  if (checksumStr.length() != 2) return false;  // Invalid checksum format

  uint8_t checksum = 0;
  for (int i = 0; i < data.length(); i++) {
    checksum ^= data[i];
  }

  uint8_t expected = strtol(checksumStr.c_str(), NULL, 16);
  return checksum == expected;
}

double nmeaCoordToDec(const String& coord, const String& hemisphere) {
  if (coord.length() < 4) {
    Serial.println("Coord too short: " + coord);
    return NAN;
  }

  int dotPos = coord.indexOf('.');
  if (dotPos < 0) {
    Serial.println("No dot in coord: " + coord);
    return NAN;
  }

  // NMEA format: latitude is DDMM.MMMM (2 digits degrees), longitude is DDDMM.MMMM (3 digits degrees)
  // Determine if this is lat or lon based on the dot position
  // Latitude: dot should be at position 4 (DDMM.M)
  // Longitude: dot should be at position 5 (DDDMM.M)
  int degLen = (dotPos == 4) ? 2 : 3;

  String degStr = coord.substring(0, degLen);
  String minStr = coord.substring(degLen);

  double degrees = degStr.toDouble();
  double minutes = minStr.toDouble();
  double decimal = degrees + (minutes / 60.0);

  // Serial.printf("Coord: %s, Hemisphere: %s\n", coord.c_str(), hemisphere.c_str());
  // Serial.printf("  Degrees: %s = %.2f, Minutes: %s = %.6f\n", degStr.c_str(), degrees, minStr.c_str(), minutes);
  // Serial.printf("  Result: %.6f\n", decimal);

  if (hemisphere == "S" || hemisphere == "W") {
    decimal = -decimal;
  }

  return decimal;
}

std::vector<String> splitNMEA(const String& sentence) {
  std::vector<String> fields;
  int start = 0;
  int idx;

  String s = sentence;
  int starPos = s.indexOf('*');
  if (starPos > 0) {
    s = s.substring(0, starPos);
  }

  while ((idx = s.indexOf(',', start)) >= 0) {
    fields.push_back(s.substring(start, idx));
    start = idx + 1;
  }
  fields.push_back(s.substring(start));

  return fields;
}

void parseNMEASentence(const String& sentence) {
  if (sentence.length() < 7 || sentence[0] != '$') return;

  // Validate checksum if present (only warn, don't reject for now)
  if (!validateNmeaChecksum(sentence)) {
    Serial.println("NMEA: Warning - checksum validation failed for: " + sentence);
    // Continue processing anyway - some devices send sentences without checksums
    // or with incorrect checksums but the data is still valid
  }

  std::vector<String> fields = splitNMEA(sentence);
  if (fields.size() < 3) return;

  String msgType = fields[0];

  // $GPRMC - Recommended Minimum
  if (msgType.endsWith("RMC") && fields.size() >= 10) {
    String status = fields[2];
    if (status != "A") return; // Not valid

    double lat = nmeaCoordToDec(fields[3], fields[4]);
    double lon = nmeaCoordToDec(fields[5], fields[6]);
    double sog = fields[7].toDouble(); // knots
    double cog = fields[8].toDouble(); // degrees

    if (!isnan(lat) && !isnan(lon)) {
      gpsData.lat = lat;
      gpsData.lon = lon;
      gpsData.timestamp = iso8601Now();

      // Only use the combined position object, not separate lat/lon paths
      updateNavigationPosition(lat, lon, "nmea0183.GPS");
    }

    if (!isnan(sog) && sog >= 0) {
      gpsData.sog = knotsToMS(sog);
      setPathValue("navigation.speedOverGround", gpsData.sog, "nmea0183.GPS", "m/s", "Speed over ground");
    }

    if (!isnan(cog) && cog >= 0 && cog <= 360) {
      gpsData.cog = degToRad(cog);
      setPathValue("navigation.courseOverGroundTrue", gpsData.cog, "nmea0183.GPS", "rad", "Course over ground (true)");
    }
  }

  // $GPGGA - GPS Fix Data
  else if (msgType.endsWith("GGA") && fields.size() >= 15) {
    double lat = nmeaCoordToDec(fields[2], fields[3]);
    double lon = nmeaCoordToDec(fields[4], fields[5]);
    String quality = fields[6];
    int sats = fields[7].toInt();
    double alt = fields[9].toDouble();

    if (!isnan(lat) && !isnan(lon)) {
      gpsData.lat = lat;
      gpsData.lon = lon;
      gpsData.satellites = sats;
      gpsData.fixQuality = quality;
      gpsData.timestamp = iso8601Now();

      // Only use the combined position object, not separate lat/lon paths
      setPathValue("navigation.gnss.satellitesInView", (double)sats, "nmea0183.GPS", "", "Satellites in view");
      updateNavigationPosition(lat, lon, "nmea0183.GPS");
    }

    if (!isnan(alt)) {
      gpsData.altitude = alt;
      setPathValue("navigation.gnss.altitude", alt, "nmea0183.GPS", "m", "Altitude");
    }
  }

  // $GPVTG - Track & Speed
  else if (msgType.endsWith("VTG") && fields.size() >= 9) {
    double cog = fields[1].toDouble();
    double sog = fields[5].toDouble(); // knots

    if (!isnan(cog) && cog >= 0 && cog <= 360) {
      gpsData.cog = degToRad(cog);
      setPathValue("navigation.courseOverGroundTrue", gpsData.cog, "nmea0183.GPS", "rad", "Course over ground");
    }

    if (!isnan(sog) && sog >= 0) {
      gpsData.sog = knotsToMS(sog);
      setPathValue("navigation.speedOverGround", gpsData.sog, "nmea0183.GPS", "m/s", "Speed over ground");
    }
  }

  // $HCHDG or $GPHDG - Heading
  else if (msgType.endsWith("HDG") && fields.size() >= 2) {
    double heading = fields[1].toDouble();
    if (!isnan(heading) && heading >= 0 && heading <= 360) {
      gpsData.heading = degToRad(heading);
      setPathValue("navigation.headingMagnetic", gpsData.heading, "nmea0183.GPS", "rad", "Heading (magnetic)");
    }
  }

  // $GPGLL - Geographic Position - Latitude/Longitude
  else if (msgType.endsWith("GLL") && fields.size() >= 7) {
    String status = fields[6];
    if (status != "A") return; // Not valid

    double lat = nmeaCoordToDec(fields[1], fields[2]);
    double lon = nmeaCoordToDec(fields[3], fields[4]);

    if (!isnan(lat) && !isnan(lon)) {
      gpsData.lat = lat;
      gpsData.lon = lon;
      gpsData.timestamp = iso8601Now();

      // Only use the combined position object, not separate lat/lon paths
      updateNavigationPosition(lat, lon, "nmea0183.GPS");
    }
  }

  // $IIHDM - Heading Magnetic
  else if (msgType.endsWith("HDM") && fields.size() >= 2) {
    double heading = fields[1].toDouble();
    if (!isnan(heading) && heading >= 0 && heading <= 360) {
      setPathValue("navigation.headingMagnetic", degToRad(heading), "nmea0183.GPS", "rad", "Heading (magnetic)");
    }
  }

  // $IIHDT - Heading True
  else if (msgType.endsWith("HDT") && fields.size() >= 2) {
    double heading = fields[1].toDouble();
    if (!isnan(heading) && heading >= 0 && heading <= 360) {
      setPathValue("navigation.headingTrue", degToRad(heading), "nmea0183.GPS", "rad", "Heading (true)");
    }
  }

  // $IIMWD - Meteorological Composite
  else if (msgType.endsWith("MWD") && fields.size() >= 8) {
    double windDirTrue = fields[1].toDouble();
    double windDirMag = fields[3].toDouble();
    double windSpeedKnots = fields[5].toDouble();
    double windSpeedMs = knotsToMS(windSpeedKnots);

    if (!isnan(windDirTrue) && windDirTrue >= 0 && windDirTrue <= 360) {
      setPathValue("environment.wind.directionTrue", degToRad(windDirTrue), "nmea0183.GPS", "rad", "Wind direction (true)");
    }
    if (!isnan(windDirMag) && windDirMag >= 0 && windDirMag <= 360) {
      setPathValue("environment.wind.directionMagnetic", degToRad(windDirMag), "nmea0183.GPS", "rad", "Wind direction (magnetic)");
    }
    if (!isnan(windSpeedMs) && windSpeedMs >= 0) {
      setPathValue("environment.wind.speedTrue", windSpeedMs, "nmea0183.GPS", "m/s", "Wind speed (true)");
      // Trigger wind alarm monitoring
      updateWindAlarm(windSpeedMs);
    }
  }

  // $IIVDR - Set and Drift
  else if (msgType.endsWith("VDR") && fields.size() >= 6) {
    double set = fields[1].toDouble();
    double drift = fields[3].toDouble();

    if (!isnan(set) && set >= 0 && set <= 360) {
      setPathValue("navigation.current.setTrue", degToRad(set), "nmea0183.GPS", "rad", "Current set (true)");
    }
    if (!isnan(drift) && drift >= 0) {
      setPathValue("navigation.current.drift", knotsToMS(drift), "nmea0183.GPS", "m/s", "Current drift");
    }
  }

  // $IIVHW - Water speed and heading
  else if (msgType.endsWith("VHW") && fields.size() >= 8) {
    double headingTrue = fields[1].toDouble();
    double headingMag = fields[3].toDouble();
    double speedKnots = fields[5].toDouble();
    double speedMs = knotsToMS(speedKnots);

    if (!isnan(headingTrue) && headingTrue >= 0 && headingTrue <= 360) {
      setPathValue("navigation.headingTrue", degToRad(headingTrue), "nmea0183.GPS", "rad", "Heading (true)");
    }
    if (!isnan(headingMag) && headingMag >= 0 && headingMag <= 360) {
      setPathValue("navigation.headingMagnetic", degToRad(headingMag), "nmea0183.GPS", "rad", "Heading (magnetic)");
    }
    if (!isnan(speedMs) && speedMs >= 0) {
      setPathValue("navigation.speedThroughWater", speedMs, "nmea0183.GPS", "m/s", "Speed through water");
    }
  }

  // $IIVPW - Speed Parallel to Wind
  else if (msgType.endsWith("VPW") && fields.size() >= 3) {
    double speedKnots = fields[1].toDouble();
    double speedMs = knotsToMS(speedKnots);

    if (!isnan(speedMs) && speedMs >= 0) {
      setPathValue("navigation.speedThroughWater", speedMs, "nmea0183.GPS", "m/s", "Speed through water");
    }
  }

  // $IIMWV - Wind Speed and Angle
  else if (msgType.endsWith("MWV") && fields.size() >= 6) {
    double windAngle = fields[1].toDouble();
    String reference = fields[2];
    double windSpeedKnots = fields[3].toDouble();
    double windSpeedMs = knotsToMS(windSpeedKnots);
    String status = fields[5];

    if (status != "A") return; // Not valid

    if (!isnan(windAngle) && windAngle >= 0 && windAngle <= 360 && !isnan(windSpeedMs) && windSpeedMs >= 0) {
      if (reference == "R") {
        // Relative wind
        setPathValue("environment.wind.angleApparent", degToRad(windAngle), "nmea0183.GPS", "rad", "Apparent wind angle");
        setPathValue("environment.wind.speedApparent", windSpeedMs, "nmea0183.GPS", "m/s", "Apparent wind speed");
      } else if (reference == "T") {
        // True wind
        setPathValue("environment.wind.angleTrueWater", degToRad(windAngle), "nmea0183.GPS", "rad", "True wind angle");
        setPathValue("environment.wind.speedTrue", windSpeedMs, "nmea0183.GPS", "m/s", "True wind speed");
        // Trigger wind alarm monitoring
        updateWindAlarm(windSpeedMs);
      }
    }
  }

  // $IIVWT - Wind Speed and Angle True
  else if (msgType.endsWith("VWT") && fields.size() >= 7) {
    double windAngleL = fields[1].toDouble();
    String dirL = fields[2];
    double windAngleR = fields[3].toDouble();
    String dirR = fields[4];
    double windSpeedKnots = fields[5].toDouble();
    double windSpeedMs = knotsToMS(windSpeedKnots);

    if (!isnan(windSpeedMs) && windSpeedMs >= 0) {
      setPathValue("environment.wind.speedTrue", windSpeedMs, "nmea0183.GPS", "m/s", "True wind speed");
      // Trigger wind alarm monitoring
      updateWindAlarm(windSpeedMs);
    }

    // Use left wind angle if available, otherwise right
    double windAngle = !isnan(windAngleL) ? windAngleL : windAngleR;
    if (!isnan(windAngle) && windAngle >= 0 && windAngle <= 360) {
      setPathValue("environment.wind.angleTrueWater", degToRad(windAngle), "nmea0183.GPS", "rad", "True wind angle");
    }
  }

  // $GPWCV - Waypoint Closure Velocity
  else if (msgType.endsWith("WCV") && fields.size() >= 4) {
    double velocityKnots = fields[1].toDouble();
    double velocityMs = knotsToMS(velocityKnots);

    if (!isnan(velocityMs) && velocityMs >= 0) {
      setPathValue("navigation.course.nextPoint.velocityMadeGood", velocityMs, "nmea0183.GPS", "m/s", "Velocity made good to waypoint");
    }
  }

  // $GPXTE - Cross-Track Error
  else if (msgType.endsWith("XTE") && fields.size() >= 6) {
    String status1 = fields[1];
    String status2 = fields[2];
    double xteNm = fields[3].toDouble();
    String direction = fields[4];

    if (status1 == "A" && status2 == "A" && !isnan(xteNm)) {
      double xteM = xteNm * 1852.0; // Convert nautical miles to meters
      if (direction == "L") xteM = -xteM; // Left is negative
      setPathValue("navigation.course.crossTrackError", xteM, "nmea0183.GPS", "m", "Cross-track error");
    }
  }

  // $GPZDA - Time & Date
  else if (msgType.endsWith("ZDA") && fields.size() >= 5) {
    int hour = fields[1].toInt();
    int minute = fields[2].toInt();
    int second = fields[3].toInt();
    int day = fields[4].toInt();
    int month = fields[5].toInt();
    int year = fields[6].toInt();

    if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
      // Could set system time here if needed
      // For now, just acknowledge the time data
    }
  }

  // $IIDBT - Depth of Water
  else if (msgType.endsWith("DBT") && fields.size() >= 7) {
    double depthFeet = fields[1].toDouble();
    double depthMeters = fields[3].toDouble();
    double depthFathoms = fields[5].toDouble();

    // Use meters if available, otherwise convert from feet
    double depth = !isnan(depthMeters) ? depthMeters : (depthFeet * 0.3048);

    if (!isnan(depth) && depth >= 0) {
      setPathValue("environment.depth.belowTransducer", depth, "nmea0183.GPS", "m", "Depth below transducer");
      // Trigger depth alarm monitoring
      updateDepthAlarm(depth);
    }
  }

  // $GPGSV - GPS Satellites in View
  else if (msgType.endsWith("GSV") && fields.size() >= 4) {
    int totalMessages = fields[1].toInt();
    int messageNumber = fields[2].toInt();
    int satellitesInView = fields[3].toInt();

    if (!isnan(satellitesInView) && satellitesInView >= 0) {
      setPathValue("navigation.gnss.satellitesInView", (double)satellitesInView, "nmea0183.GPS", "", "Satellites in view");
    }
  }
}
