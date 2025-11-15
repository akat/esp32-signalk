#include "nmea2000.h"
#include <ArduinoJson.h>
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

    // Broadcast NMEA 0183 sentences via TCP
    static uint32_t lastGGA = 0;
    uint32_t now = millis();
    if (now - lastGGA > 1000) {  // Send GGA once per second
      lastGGA = now;
      String gga = convertToGGA(latitude, longitude, gpsData.timestamp, gpsData.satellites, gpsData.altitude);
      broadcastNMEA0183(gga);

      String gll = convertToGLL(latitude, longitude, gpsData.timestamp);
      broadcastNMEA0183(gll);
    }
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

    // Broadcast NMEA 0183 sentences via TCP
    static uint32_t lastVTG = 0;
    uint32_t now = millis();
    if (!N2kIsNA(COG) && !N2kIsNA(SOG) && (now - lastVTG > 1000)) {  // Send VTG once per second
      lastVTG = now;
      String vtg = convertToVTG(COG, SOG);
      broadcastNMEA0183(vtg);

      // Send RMC if we have position data too
      if (gpsData.lat != 0.0 && gpsData.lon != 0.0) {
        String rmc = convertToRMC(gpsData.lat, gpsData.lon, COG, SOG, gpsData.timestamp);
        broadcastNMEA0183(rmc);
      }
    }
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

      // Broadcast NMEA 0183 MWV sentence via TCP
      static uint32_t lastMWV = 0;
      uint32_t now = millis();
      if (!N2kIsNA(WindSpeed) && !N2kIsNA(WindAngle) && (now - lastMWV > 200)) {  // Send MWV at 5Hz
        lastMWV = now;
        String mwv = convertToMWV(WindAngle, WindSpeed, 'T');  // T = True wind
        broadcastNMEA0183(mwv);
      }
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

      // Broadcast NMEA 0183 DPT sentence via TCP
      static uint32_t lastDPT = 0;
      uint32_t now = millis();
      if (now - lastDPT > 500) {  // Send DPT at 2Hz
        lastDPT = now;
        double offset = N2kIsNA(Offset) ? 0.0 : Offset;
        String dpt = convertToDPT(DepthBelowTransducer, offset);
        broadcastNMEA0183(dpt);
      }
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

      // Broadcast NMEA 0183 MTW sentence via TCP
      static uint32_t lastMTW = 0;
      uint32_t now = millis();
      if (now - lastMTW > 2000) {  // Send MTW every 2 seconds
        lastMTW = now;
        String mtw = convertToMTW(WaterTemperature);
        broadcastNMEA0183(mtw);
      }
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

  // Enable forward mode to see ALL raw NMEA2000 messages on Serial
  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);
  NMEA2000.SetForwardStream(&Serial);
  NMEA2000.EnableForward(true);  // Make sure forwarding is enabled
  Serial.println("Forward mode enabled - will show ALL N2K messages on bus");

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
