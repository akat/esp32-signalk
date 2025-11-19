#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include <Arduino.h>

// Unit conversions
double knotsToMS(double knots);
double degToRad(double deg);

// Geospatial calculations
double haversineDistance(double lat1, double lon1, double lat2, double lon2);

#endif // CONVERSIONS_H
