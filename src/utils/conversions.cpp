#include "conversions.h"
#include <math.h>

double knotsToMS(double knots) {
  return knots * 0.514444;
}

double degToRad(double deg) {
  return deg * M_PI / 180.0;
}

// Haversine distance calculation (returns meters)
double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0; // Earth radius in meters
  const double toRad = M_PI / 180.0;

  double dLat = (lat2 - lat1) * toRad;
  double dLon = (lon2 - lon1) * toRad;
  double a = sin(dLat/2) * sin(dLat/2) +
             cos(lat1 * toRad) * cos(lat2 * toRad) *
             sin(dLon/2) * sin(dLon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}
