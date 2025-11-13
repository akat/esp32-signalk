#include "nmea2000.h"
#include <ArduinoJson.h>
#include <NMEA2000.h>
#include <N2kMessages.h>

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

    DynamicJsonDocument posDoc(128);
    posDoc["latitude"] = latitude;
    posDoc["longitude"] = longitude;
    String posJson;
    serializeJson(posDoc, posJson);
    setPathValue("navigation.position", posJson, "nmea2000.can", "", "Vessel position");

    Serial.printf("N2K Position: %.6f, %.6f\n", latitude, longitude);
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

    Serial.printf("N2K COG/SOG: %.1f deg, %.2f m/s\n", RadToDeg(COG), SOG);
  }
}

void HandleN2kWindSpeed(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double WindSpeed, WindAngle;
  tN2kWindReference WindReference;

  if (ParseN2kPGN130306(N2kMsg, SID, WindSpeed, WindAngle, WindReference)) {
    if (WindReference == N2kWind_True_water || WindReference == N2kWind_True_North) {
      if (!N2kIsNA(WindSpeed)) {
        setPathValue("environment.wind.speedTrue", WindSpeed, "nmea2000.can", "m/s", "True wind speed");
        updateWindAlarm(WindSpeed);
      }
      if (!N2kIsNA(WindAngle)) {
        setPathValue("environment.wind.angleTrueWater", WindAngle, "nmea2000.can", "rad", "True wind angle");
      }
      Serial.printf("N2K Wind: %.1f m/s at %.0f deg\n", WindSpeed, RadToDeg(WindAngle));
    }
  }
}

void HandleN2kWaterDepth(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double DepthBelowTransducer, Offset, Range;

  if (ParseN2kPGN128267(N2kMsg, SID, DepthBelowTransducer, Offset, Range)) {
    if (!N2kIsNA(DepthBelowTransducer)) {
      setPathValue("environment.depth.belowTransducer", DepthBelowTransducer, "nmea2000.can", "m", "Depth below transducer");
      updateDepthAlarm(DepthBelowTransducer);
      Serial.printf("N2K Depth: %.1f m\n", DepthBelowTransducer);
    }
  }
}

void HandleN2kOutsideEnvironment(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  double WaterTemperature, OutsideAmbientAirTemperature, AtmosphericPressure;

  if (ParseN2kPGN130310(N2kMsg, SID, WaterTemperature, OutsideAmbientAirTemperature, AtmosphericPressure)) {
    if (!N2kIsNA(WaterTemperature)) {
      double tempC = KelvinToC(WaterTemperature);
      setPathValue("environment.water.temperature", WaterTemperature, "nmea2000.can", "K", "Water temperature");
      Serial.printf("N2K Water Temp: %.1f°C\n", tempC);
    }
    if (!N2kIsNA(OutsideAmbientAirTemperature)) {
      setPathValue("environment.outside.temperature", OutsideAmbientAirTemperature, "nmea2000.can", "K", "Outside air temperature");
    }
    if (!N2kIsNA(AtmosphericPressure)) {
      setPathValue("environment.outside.pressure", AtmosphericPressure, "nmea2000.can", "Pa", "Atmospheric pressure");
    }
  }
}

void initNMEA2000() {
  Serial.println("Initializing NMEA2000...");

  // Set Product information
  NMEA2000.SetProductInformation("00000001", // Manufacturer's Model serial code
                                  100, // Manufacturer's product code
                                  "ESP32 SignalK Gateway", // Manufacturer's Model ID
                                  "1.0.0", // Manufacturer's Software version code
                                  "1.0.0" // Manufacturer's Model version
                                 );

  // Set Device information
  NMEA2000.SetDeviceInformation(1, // Unique number
                                 130, // Device function=Display
                                 25, // Device class=Network Device
                                 2046 // Just choosen free from code list
                                );

  // Set message handlers
  NMEA2000.SetMsgHandler(HandleN2kPosition);
  NMEA2000.SetMsgHandler(HandleN2kCOGSOG);
  NMEA2000.SetMsgHandler(HandleN2kWindSpeed);
  NMEA2000.SetMsgHandler(HandleN2kWaterDepth);
  NMEA2000.SetMsgHandler(HandleN2kOutsideEnvironment);

  // Enable forward mode (listen only, don't claim address)
  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);
  NMEA2000.SetForwardStream(&Serial);
  NMEA2000.SetMode(tNMEA2000::N2km_ListenOnly, 25);

  if (NMEA2000.Open()) {
    Serial.println("NMEA2000 initialized successfully");
    n2kEnabled = true;
  } else {
    Serial.println("NMEA2000 initialization failed");
    n2kEnabled = false;
  }
}
