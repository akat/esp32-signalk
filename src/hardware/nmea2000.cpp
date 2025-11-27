#include "nmea2000.h"
#include <NMEA2000.h>
#include <N2kMessages.h>
#include "../config.h"
#include "../utils/nmea0183_converter.h"
#include "../services/nmea0183_tcp.h"

// External declaration for NMEA2000 instance (defined in main.cpp)
class tNMEA2000;
extern tNMEA2000& NMEA2000;

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

extern bool n2kEnabled;

// Helper functions (declared in main)
extern String iso8601Now();
extern void setPathValue(const String& path, double value, const String& source,
                        const String& unit, const String& description);
extern void setPathValue(const String& path, const String& value, const String& source,
                        const String& unit, const String& description);
extern void updateNavigationPosition(double lat, double lon, const String& source);
extern void updateWindAlarm(double windSpeedMS);
extern void updateDepthAlarm(double depth);
extern double RadToDeg(double rad);
extern double KelvinToC(double kelvin);

// ====== NMEA 2000 MESSAGE HANDLERS ======

void HandleN2kPosition(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double latitude, longitude;

  if (ParseN2kPGN129025(N2kMsg, latitude, longitude)) {
    gpsData.lat = latitude;
    gpsData.lon = longitude;
    gpsData.timestamp = iso8601Now();

    updateNavigationPosition(latitude, longitude, "nmea2000.can");

    // NMEA 0183 broadcasting is now done from dataStore in main loop
    // This ensures per-path priority filtering
  }
}

void HandleN2kCOGSOG(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  tN2kHeadingReference HeadingReference;
  double COG, SOG;

  if (ParseN2kPGN129026(N2kMsg, SID, HeadingReference, COG, SOG)) {
    if (!N2kIsNA(COG)) {
      gpsData.cog = COG;
      setPathValue("navigation.courseOverGroundTrue", COG, "nmea2000.can", "rad", "Course over ground");
    }
    if (!N2kIsNA(SOG)) {
      gpsData.sog = SOG;
      setPathValue("navigation.speedOverGround", SOG, "nmea2000.can", "m/s", "Speed over ground");
    }

    // NMEA 0183 broadcasting is now done from dataStore in main loop
  }
}

void HandleN2kWindSpeed(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double WindSpeed, WindAngle;
  tN2kWindReference WindReference;

  if (ParseN2kPGN130306(N2kMsg, SID, WindSpeed, WindAngle, WindReference)) {
    if (WindReference == N2kWind_Apparent) {
      if (!N2kIsNA(WindSpeed)) {
        setPathValue("environment.wind.speedApparent", WindSpeed, "nmea2000.can", "m/s", "Apparent wind speed");
      }
      if (!N2kIsNA(WindAngle)) {
        setPathValue("environment.wind.angleApparent", WindAngle, "nmea2000.can", "rad", "Apparent wind angle");
      }
    } else if (WindReference == N2kWind_True_water || WindReference == N2kWind_True_North) {
      if (!N2kIsNA(WindSpeed)) {
        setPathValue("environment.wind.speedTrue", WindSpeed, "nmea2000.can", "m/s", "True wind speed");
        updateWindAlarm(WindSpeed);
      }
      if (!N2kIsNA(WindAngle)) {
        setPathValue("environment.wind.angleTrueWater", WindAngle, "nmea2000.can", "rad", "True wind angle");
      }
    }

    // NMEA 0183 broadcasting is now done from dataStore in main loop
  }
}

void HandleN2kWaterDepth(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double DepthBelowTransducer, Offset, Range;

  if (ParseN2kPGN128267(N2kMsg, SID, DepthBelowTransducer, Offset, Range)) {
    if (!N2kIsNA(DepthBelowTransducer)) {
      setPathValue("environment.depth.belowTransducer", DepthBelowTransducer, "nmea2000.can", "m", "Depth below transducer");
      updateDepthAlarm(DepthBelowTransducer);

      // NMEA 0183 broadcasting is now done from dataStore in main loop
    }
  }
}

void HandleN2kOutsideEnvironment(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double WaterTemperature, OutsideAmbientAirTemperature, AtmosphericPressure;

  if (ParseN2kPGN130310(N2kMsg, SID, WaterTemperature, OutsideAmbientAirTemperature, AtmosphericPressure)) {
    if (!N2kIsNA(WaterTemperature)) {
      setPathValue("environment.water.temperature", WaterTemperature, "nmea2000.can", "K", "Water temperature");

      // NMEA 0183 broadcasting is now done from dataStore in main loop
    }
    if (!N2kIsNA(OutsideAmbientAirTemperature)) {
      setPathValue("environment.outside.temperature", OutsideAmbientAirTemperature, "nmea2000.can", "K", "Outside air temperature");
    }
    if (!N2kIsNA(AtmosphericPressure)) {
      setPathValue("environment.outside.pressure", AtmosphericPressure, "nmea2000.can", "Pa", "Atmospheric pressure");
    }
  }
}
 
// Central dispatcher for the general SetMsgHandler callback.
static void HandleN2kMessage(const tN2kMsg &N2kMsg) {
  switch (N2kMsg.PGN) {
    case 129025UL:
      HandleN2kPosition(N2kMsg);
      break;
    case 129026UL:
      HandleN2kCOGSOG(N2kMsg);
      break;
    case 130306UL:
      HandleN2kWindSpeed(N2kMsg);
      break;
    case 128267UL:
      HandleN2kWaterDepth(N2kMsg);
      break;
    case 130310UL:
      HandleN2kOutsideEnvironment(N2kMsg);
      break;
    default:
      break;
  }
}

void initNMEA2000() {
  Serial.println("\n=== Initializing NMEA2000 CAN Bus ===");

  // Enable CAN transceiver on TTGO T-CAN485 (SE pin must be LOW to enable)
  pinMode(CAN_SE_PIN, OUTPUT);
  digitalWrite(CAN_SE_PIN, LOW);  // LOW = transceiver enabled
  Serial.println("CAN transceiver enabled (SE pin LOW)");

  // Product information (what chartplotters will display)
  NMEA2000.SetProductInformation(
    "ESP32N2K-0001",   // Serial code
    100,               // Product code
    "ESP32 N2K Gateway",
    "1.0.0",           // Software version
    "HW:1.0"           // Hardware version
  );
  Serial.println("Product info set");

  // Device information (PGN 60928/126998 responses)
  NMEA2000.SetDeviceInformation(
    123,   // Unique number
    140,   // Device function = Generic Display
    25,    // Device class = Display
    2046   // Manufacturer code reserved for experiments
  );
  Serial.println("Device info set");

  // Single dispatcher registered with the library
  NMEA2000.SetMsgHandler(HandleN2kMessage);
  Serial.println("Message handler registered");

  // Forward mode disabled for production (too verbose)
  NMEA2000.EnableForward(false);

  const uint8_t preferredAddress = 25;  // Recommended gateway address
  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, preferredAddress);
  NMEA2000.SetN2kCANMsgBufSize(60);
  NMEA2000.SetN2kCANSendFrameBufSize(30);
  Serial.println("Mode: NodeOnly (address claim enabled)");

  if (NMEA2000.Open()) {
    Serial.println("NMEA2000 CAN bus opened successfully");
    Serial.println("\nConfiguration:");
    Serial.println("  - Mode: NodeOnly (address claim + talker)");
    Serial.printf("  - Preferred address: %u\n", preferredAddress);
    Serial.printf("  - CAN TX Pin: %d\n", static_cast<int>(CAN_TX_PIN));
    Serial.printf("  - CAN RX Pin: %d\n", static_cast<int>(CAN_RX_PIN));
    Serial.println("  - Forwarding: text to Serial");
    Serial.println("======================================\n");
    n2kEnabled = true;
  } else {
    Serial.println("ERROR: NMEA2000 CAN bus initialization FAILED!");
    Serial.println("This means the CAN hardware is not responding!");
    Serial.println("\nPossible causes:");
    Serial.println("  - Wrong GPIO pins or wiring");
    Serial.println("  - CAN transceiver chip failure");
    Serial.println("  - No power or missing ground on CAN backbone");
    Serial.println("======================================\n");
    n2kEnabled = false;
  }
}
